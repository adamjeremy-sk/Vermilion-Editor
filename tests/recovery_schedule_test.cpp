// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery copy schedule: due times, the cap, the back-off and one copy at a time.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <vector>

#include "core/recovery_schedule.h"

namespace {

using vermilion::DocumentId;
using vermilion::Millis;
using vermilion::RecoverySchedule;

constexpr Millis kMax = std::numeric_limits<Millis>::max();
constexpr Millis kMin = std::numeric_limits<Millis>::min();

TEST(RecoverySchedule, TheDecidedValues) {
    EXPECT_EQ(vermilion::kRecoveryIdleMs, Millis(5000));
    EXPECT_EQ(vermilion::kRecoveryCapMs, Millis(30000));
    EXPECT_EQ(vermilion::kRecoveryBackoffFactor, Millis(20));
    EXPECT_EQ(vermilion::kRecoveryMaxFreezeMs, Millis(3600000));
}

TEST(RecoverySchedule, TheSizeCeilingIsInclusive) {
    EXPECT_TRUE(vermilion::recovery_size_allowed(0));
    EXPECT_TRUE(vermilion::recovery_size_allowed(vermilion::kRecoveryCeilingChars));
    EXPECT_FALSE(vermilion::recovery_size_allowed(vermilion::kRecoveryCeilingChars + 1));
    EXPECT_FALSE(vermilion::recovery_size_allowed(std::numeric_limits<std::uint64_t>::max()));
}

TEST(RecoverySchedule, NothingIsDueWithoutChanges) {
    RecoverySchedule schedule;
    EXPECT_FALSE(schedule.next_due(0).has_value());
    EXPECT_FALSE(schedule.next_due(kMax).has_value());
    EXPECT_FALSE(schedule.wake_at().has_value());
    EXPECT_FALSE(schedule.copy_in_flight());
    EXPECT_FALSE(schedule.pending(1));
}

TEST(RecoverySchedule, ACopyIsDueFiveSecondsAfterTheLastChange) {
    RecoverySchedule schedule;
    schedule.note_change(1, 1000);
    EXPECT_TRUE(schedule.pending(1));
    EXPECT_FALSE(schedule.next_due(5999).has_value());
    EXPECT_EQ(schedule.next_due(6000), std::optional<DocumentId>(1));
    EXPECT_EQ(schedule.next_due(99999), std::optional<DocumentId>(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(6000));
}

TEST(RecoverySchedule, TypingKeepsPushingTheCopyBack) {
    RecoverySchedule schedule;
    for (Millis t : {Millis(0), Millis(3000), Millis(6000)}) {
        schedule.note_change(1, t);
    }
    EXPECT_FALSE(schedule.next_due(10999).has_value());
    EXPECT_EQ(schedule.next_due(11000), std::optional<DocumentId>(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(11000));
}

TEST(RecoverySchedule, TheCapWinsWhileTypingContinues) {
    RecoverySchedule schedule;
    for (Millis t = 0; t <= 28000; t += 4000) {
        schedule.note_change(1, t);
    }
    EXPECT_FALSE(schedule.next_due(29999).has_value());
    EXPECT_EQ(schedule.next_due(30000), std::optional<DocumentId>(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(30000));
}

TEST(RecoverySchedule, TheCapCountsFromTheFirstChangeSinceTheLastCopy) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(100, 0, true));
    schedule.note_change(1, 20000);
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(25000));
    for (Millis t = 24000; t <= 48000; t += 4000) {
        schedule.note_change(1, t);
    }
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(50000));
}

TEST(RecoverySchedule, TheEarliestDocumentGoesFirstAndTheLowestIdBreaksATie) {
    RecoverySchedule schedule;
    schedule.note_change(2, 0);
    schedule.note_change(1, 1000);
    schedule.note_change(3, 0);
    EXPECT_EQ(schedule.next_due(50000), std::optional<DocumentId>(2));
    ASSERT_TRUE(schedule.begin_copy(2));
    EXPECT_FALSE(schedule.end_copy(50000, 0, true));
    EXPECT_EQ(schedule.next_due(50000), std::optional<DocumentId>(3));
    ASSERT_TRUE(schedule.begin_copy(3));
    EXPECT_FALSE(schedule.end_copy(50000, 0, true));
    EXPECT_EQ(schedule.next_due(50000), std::optional<DocumentId>(1));
}

TEST(RecoverySchedule, OneCopyAtATime) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    schedule.note_change(2, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_TRUE(schedule.copy_in_flight());
    EXPECT_FALSE(schedule.pending(1));
    EXPECT_FALSE(schedule.begin_copy(2));
    EXPECT_TRUE(schedule.pending(2));
    EXPECT_FALSE(schedule.next_due(kMax).has_value());
    EXPECT_FALSE(schedule.wake_at().has_value());
    EXPECT_FALSE(schedule.end_copy(9000, 0, true));
    EXPECT_FALSE(schedule.copy_in_flight());
    EXPECT_EQ(schedule.next_due(9000), std::optional<DocumentId>(2));
}

TEST(RecoverySchedule, BeginCopyRefusesADocumentThatIsNotPending) {
    RecoverySchedule schedule;
    EXPECT_FALSE(schedule.begin_copy(1));
    schedule.note_change(1, 0);
    schedule.forget(1);
    EXPECT_FALSE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.copy_in_flight());
}

TEST(RecoverySchedule, TheNextCopyWaitsTwentyTimesTheFreeze) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    schedule.note_change(2, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(10000, 250, true));
    EXPECT_FALSE(schedule.next_due(14999).has_value());
    EXPECT_EQ(schedule.next_due(15000), std::optional<DocumentId>(2));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(15000));
}

TEST(RecoverySchedule, ABackOffThatEndsBeforeTheDueTimeChangesNothing) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(1000, 100, true));
    schedule.note_change(2, 1000);
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(6000));
    EXPECT_EQ(schedule.next_due(6000), std::optional<DocumentId>(2));
}

TEST(RecoverySchedule, TheFreezeIsClamped) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(1000, -500, true));
    schedule.note_change(2, 0);
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(5000));
    ASSERT_TRUE(schedule.begin_copy(2));
    EXPECT_FALSE(schedule.end_copy(6000, kMax, true));
    schedule.note_change(3, 0);
    Millis limit = 6000 + vermilion::kRecoveryMaxFreezeMs * vermilion::kRecoveryBackoffFactor;
    EXPECT_FALSE(schedule.next_due(limit - 1).has_value());
    EXPECT_EQ(schedule.next_due(limit), std::optional<DocumentId>(3));
}

TEST(RecoverySchedule, ForgetDropsAPendingDocument) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    schedule.note_change(2, 0);
    schedule.forget(1);
    schedule.forget(77);
    EXPECT_FALSE(schedule.pending(1));
    EXPECT_EQ(schedule.next_due(kMax), std::optional<DocumentId>(2));
    schedule.forget(2);
    EXPECT_FALSE(schedule.next_due(kMax).has_value());
    EXPECT_FALSE(schedule.wake_at().has_value());
}

TEST(RecoverySchedule, ForgettingADocumentUnderCopyMakesItsCopyStale) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    schedule.forget(1);
    EXPECT_TRUE(schedule.copy_in_flight());
    EXPECT_TRUE(schedule.end_copy(100, 0, true));
    EXPECT_FALSE(schedule.pending(1));
    schedule.note_change(2, 200);
    ASSERT_TRUE(schedule.begin_copy(2));
    schedule.forget(1);
    EXPECT_FALSE(schedule.end_copy(300, 0, true));
}

TEST(RecoverySchedule, AChangeDuringACopyKeepsTheDocumentPending) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    schedule.note_change(1, 2000);
    EXPECT_FALSE(schedule.end_copy(3000, 0, true));
    EXPECT_TRUE(schedule.pending(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(7000));
}

TEST(RecoverySchedule, ADocumentForgottenAndChangedAgainDuringACopyIsStaleAndPending) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    schedule.forget(1);
    schedule.note_change(1, 4000);
    EXPECT_TRUE(schedule.end_copy(5000, 0, true));
    EXPECT_TRUE(schedule.pending(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(9000));
}

TEST(RecoverySchedule, AFailedWriteQueuesTheDocumentAgain) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(9000, 0, false));
    EXPECT_TRUE(schedule.pending(1));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(14000));
    ASSERT_TRUE(schedule.begin_copy(1));
    schedule.note_change(1, 15000);
    EXPECT_FALSE(schedule.end_copy(16000, 0, false));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(20000));
}

TEST(RecoverySchedule, AFailedWriteStartsTheCapAgainFromTheFailure) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(40000, 0, false));
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(45000));
    EXPECT_FALSE(schedule.next_due(44999).has_value());
    for (Millis t = 44000; t <= 68000; t += 4000) {
        schedule.note_change(1, t);
    }
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(70000));
}

TEST(RecoverySchedule, AFailedWriteOfAForgottenDocumentQueuesNothing) {
    RecoverySchedule schedule;
    schedule.note_change(1, 0);
    ASSERT_TRUE(schedule.begin_copy(1));
    schedule.forget(1);
    EXPECT_TRUE(schedule.end_copy(100, 0, false));
    EXPECT_FALSE(schedule.pending(1));
    EXPECT_FALSE(schedule.wake_at().has_value());
}

TEST(RecoverySchedule, EndCopyWithNoCopyUnderWayDoesNothing) {
    RecoverySchedule schedule;
    EXPECT_FALSE(schedule.end_copy(1000, 5000, true));
    schedule.note_change(1, 0);
    EXPECT_EQ(schedule.next_due(5000), std::optional<DocumentId>(1));
    EXPECT_FALSE(schedule.end_copy(5000, 5000, false));
    EXPECT_FALSE(schedule.pending(2));
    EXPECT_EQ(schedule.next_due(5000), std::optional<DocumentId>(1));
}

TEST(RecoverySchedule, AClockGoingBackwardsNeverMovesTimesBack) {
    RecoverySchedule schedule;
    schedule.note_change(1, 10000);
    schedule.note_change(1, 2000);
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(15000));
    EXPECT_FALSE(schedule.next_due(14999).has_value());
    EXPECT_EQ(schedule.next_due(15000), std::optional<DocumentId>(1));
}

TEST(RecoverySchedule, TimesAtTheLimitsDoNotOverflow) {
    RecoverySchedule schedule;
    schedule.note_change(1, kMax - 10);
    EXPECT_EQ(schedule.wake_at(), std::optional<Millis>(kMax));
    EXPECT_EQ(schedule.next_due(kMax), std::optional<DocumentId>(1));
    EXPECT_FALSE(schedule.next_due(kMax - 1).has_value());
    ASSERT_TRUE(schedule.begin_copy(1));
    EXPECT_FALSE(schedule.end_copy(kMax - 5, kMax, true));
    schedule.note_change(2, 0);
    EXPECT_FALSE(schedule.next_due(kMax - 1).has_value());
    EXPECT_EQ(schedule.next_due(kMax), std::optional<DocumentId>(2));
    RecoverySchedule low;
    low.note_change(5, kMin);
    EXPECT_EQ(low.wake_at(), std::optional<Millis>(kMin + vermilion::kRecoveryIdleMs));
    EXPECT_EQ(low.next_due(kMin + vermilion::kRecoveryIdleMs), std::optional<DocumentId>(5));
}

// Change times that repeat, so that equal times (and the lowest id first) are exercised.
Millis spread_time(DocumentId id) {
    return static_cast<Millis>((id * 7919) % 100);
}

TEST(RecoverySchedule, ManyDocumentsAreCopiedOnceEachInDueOrder) {
    RecoverySchedule schedule;
    const DocumentId kCount = 1000;
    for (DocumentId id = 1; id <= kCount; ++id) {
        schedule.note_change(id, spread_time(id));
    }
    std::vector<DocumentId> order;
    std::set<DocumentId> seen;
    Millis now = 100000;
    while (std::optional<DocumentId> id = schedule.next_due(now)) {
        ASSERT_TRUE(seen.insert(*id).second);
        order.push_back(*id);
        ASSERT_TRUE(schedule.begin_copy(*id));
        EXPECT_FALSE(schedule.end_copy(now, 0, true));
    }
    EXPECT_EQ(order.size(), std::size_t(kCount));
    for (std::size_t i = 1; i < order.size(); ++i) {
        Millis before = spread_time(order[i - 1]);
        Millis after = spread_time(order[i]);
        EXPECT_TRUE(before < after || (before == after && order[i - 1] < order[i])) << i;
    }
}

// A small deterministic random source, so a failure can be repeated.
class Random {
public:
    std::uint32_t next(std::uint32_t below) {
        state_ = state_ * 1664525u + 1013904223u;
        return (state_ >> 8) % below;
    }

private:
    std::uint32_t state_ = 4242;
};

// wake_at() and next_due() agree, and a document named by next_due() can always begin a copy.
TEST(RecoverySchedule, WakeAtAndNextDueAlwaysAgree) {
    Random random;
    RecoverySchedule schedule;
    Millis now = 0;
    for (int step = 0; step < 20000; ++step) {
        now += static_cast<Millis>(random.next(4000));
        DocumentId id = random.next(6) + 1;
        switch (random.next(6)) {
        case 0:
        case 1:
            schedule.note_change(id, now);
            break;
        case 2:
            schedule.forget(id);
            break;
        case 3:
            schedule.begin_copy(id);
            break;
        case 4:
            schedule.end_copy(now, static_cast<Millis>(random.next(300)), random.next(3) != 0);
            break;
        default:
            break;
        }
        std::optional<Millis> wake = schedule.wake_at();
        if (!wake) {
            EXPECT_TRUE(schedule.copy_in_flight() || !schedule.next_due(kMax).has_value());
            continue;
        }
        ASSERT_FALSE(schedule.copy_in_flight());
        std::optional<DocumentId> due = schedule.next_due(*wake);
        ASSERT_TRUE(due.has_value());
        EXPECT_FALSE(schedule.next_due(*wake - 1).has_value()) << step;
        EXPECT_TRUE(schedule.pending(*due));
    }
}

} // namespace
