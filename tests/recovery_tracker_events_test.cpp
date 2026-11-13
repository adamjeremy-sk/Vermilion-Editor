// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery tracker's copies in time: order, back-off, failures, saves and closes.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_tracker.h"
#include "recovery_tracker_test_util.h"

namespace {

using tracker_test::nothing;
using tracker_test::RecoveryTrackerTest;
using vermilion::DocumentId;
using vermilion::Millis;
using vermilion::RecoveryEffects;
using vermilion::RecoveryJob;
using Ids = std::vector<std::string>;

TEST_F(RecoveryTrackerTest, OneCopyAtATime) {
    DocumentId first = edited_untitled(0);
    DocumentId second = edited_untitled(0);
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->document, first);
    EXPECT_FALSE(begin(100000).has_value());
    EXPECT_FALSE(tracker_.wake_at().has_value());
    tracker_.end_copy(5000, 0, true);
    std::optional<RecoveryJob> next = begin(5000);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->document, second);
}

TEST_F(RecoveryTrackerTest, TheDocumentThatWaitedLongestGoesFirst) {
    DocumentId a = edited_untitled(2000);
    DocumentId b = edited_untitled(1000);
    EXPECT_EQ(begin(10000)->document, b);
    tracker_.end_copy(10000, 0, true);
    EXPECT_EQ(begin(10000)->document, a);
}

TEST_F(RecoveryTrackerTest, TheNextCopyWaitsTwentyTimesTheHold) {
    DocumentId id = edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    tracker_.end_copy(5000, 1000, true);
    tracker_.note_edit(id, 10, 5000);
    EXPECT_EQ(tracker_.wake_at(), std::optional<Millis>(25000));
    EXPECT_FALSE(begin(24999).has_value());
    EXPECT_TRUE(begin(25000).has_value());
}

TEST_F(RecoveryTrackerTest, AWrittenCopyIsRemembered) {
    edited_untitled();
    EXPECT_TRUE(tracker_.copy_ids().empty());
    ASSERT_TRUE(begin(5000).has_value());
    EXPECT_TRUE(tracker_.copy_in_flight());
    EXPECT_TRUE(nothing(tracker_.end_copy(5000, 0, true)));
    EXPECT_FALSE(tracker_.copy_in_flight());
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
}

TEST_F(RecoveryTrackerTest, EveryCopyThatMayExistIsListedOnceInOrder) {
    DocumentId b = list_.add_untitled();
    DocumentId a = list_.add_untitled();
    list_.set_modified(a, true);
    list_.set_modified(b, true);
    tracker_.note_edit(b, 10, 0);
    tracker_.note_edit(a, 10, 1000);
    EXPECT_EQ(begin(6000)->document, b);
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
    tracker_.end_copy(6000, 0, true);
    EXPECT_EQ(begin(6000)->document, a);
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1", "id-2"}));
    tracker_.end_copy(6000, 0, true);
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1", "id-2"}));
}

TEST_F(RecoveryTrackerTest, AFailedWriteIsAnnouncedOnceUntilOneWorks) {
    edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    RecoveryEffects failed = tracker_.end_copy(5000, 0, false);
    EXPECT_TRUE(failed.announce_failure);
    EXPECT_FALSE(failed.delete_id.has_value());
    EXPECT_TRUE(tracker_.copy_ids().empty());
    ASSERT_TRUE(begin(10000).has_value());
    EXPECT_FALSE(tracker_.end_copy(10000, 0, false).announce_failure);
    ASSERT_TRUE(begin(15000).has_value());
    EXPECT_TRUE(nothing(tracker_.end_copy(15000, 0, true)));
    DocumentId again = edited_untitled(16000);
    ASSERT_EQ(begin(21000)->document, again);
    EXPECT_TRUE(tracker_.end_copy(21000, 0, false).announce_failure);
}

TEST_F(RecoveryTrackerTest, AFailedWriteKeepsAnOlderCopy) {
    edited_untitled();
    write_copy(5000);
    tracker_.note_edit(1, 10, 6000);
    ASSERT_TRUE(begin(11000).has_value());
    EXPECT_FALSE(tracker_.end_copy(11000, 0, false).delete_id.has_value());
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
}

TEST_F(RecoveryTrackerTest, ACopyEndingWithoutOneBeginningChangesNothing) {
    EXPECT_TRUE(nothing(tracker_.end_copy(0, 0, false)));
    EXPECT_TRUE(nothing(tracker_.end_copy(0, 0, true)));
    edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    EXPECT_TRUE(tracker_.end_copy(5000, 0, false).announce_failure);
}

TEST_F(RecoveryTrackerTest, SavingDeletesTheCopy) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    RecoveryEffects effects = tracker_.note_clean(id);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_FALSE(effects.announce_too_large);
    EXPECT_FALSE(effects.announce_failure);
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_TRUE(nothing(tracker_.note_clean(id)));
    EXPECT_FALSE(tracker_.wake_at().has_value());
}

TEST_F(RecoveryTrackerTest, ACleanDocumentThatNeverHadACopyDeletesNothingAndIsNotCopied) {
    DocumentId id = edited_untitled();
    EXPECT_TRUE(nothing(tracker_.note_clean(id)));
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(begin(100000).has_value());
}

TEST_F(RecoveryTrackerTest, ACleanDuringTheFirstCopyDeletesWhenItEnds) {
    DocumentId id = edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    EXPECT_TRUE(nothing(tracker_.note_clean(id)));
    RecoveryEffects effects = tracker_.end_copy(5000, 0, true);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_FALSE(tracker_.wake_at().has_value());
}

TEST_F(RecoveryTrackerTest, ACleanDuringACopyOverAnOlderOneDeletesTwice) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    tracker_.note_edit(id, 10, 6000);
    ASSERT_TRUE(begin(11000).has_value());
    EXPECT_EQ(tracker_.note_clean(id).delete_id, std::optional<std::string>("id-1"));
    EXPECT_EQ(tracker_.end_copy(11000, 0, true).delete_id, std::optional<std::string>("id-1"));
    EXPECT_TRUE(tracker_.copy_ids().empty());
}

TEST_F(RecoveryTrackerTest, EditingAgainAfterASaveMakesANewCopyWithTheSameId) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    tracker_.note_clean(id);
    list_.set_modified(id, false);
    list_.set_modified(id, true);
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 10, 20000)));
    EXPECT_EQ(made_, 1);
    std::optional<RecoveryJob> job = begin(25000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->recovery_id, "id-1");
}

TEST_F(RecoveryTrackerTest, AnEditDuringACopyQueuesAnotherCopy) {
    DocumentId id = edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    tracker_.note_edit(id, 10, 6000);
    EXPECT_TRUE(nothing(tracker_.end_copy(7000, 0, true)));
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
    EXPECT_EQ(tracker_.wake_at(), std::optional<Millis>(11000));
    EXPECT_FALSE(begin(10999).has_value());
    EXPECT_TRUE(begin(11000).has_value());
}

TEST_F(RecoveryTrackerTest, ClosingDeletesTheCopyAndForgetsTheDocument) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    RecoveryEffects effects = tracker_.note_closed(id);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_FALSE(tracker_.recovery_id_of(id).has_value());
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_TRUE(nothing(tracker_.note_closed(id)));
}

TEST_F(RecoveryTrackerTest, ClosingDuringACopyDeletesWhenItEnds) {
    DocumentId id = edited_untitled();
    ASSERT_TRUE(begin(5000).has_value());
    EXPECT_TRUE(nothing(tracker_.note_closed(id)));
    list_.close(id);
    EXPECT_TRUE(tracker_.copy_in_flight());
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
    RecoveryEffects effects = tracker_.end_copy(5000, 0, true);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_FALSE(tracker_.wake_at().has_value());
}

TEST_F(RecoveryTrackerTest, AClosedDocumentIsNeverCopied) {
    DocumentId id = edited_untitled();
    tracker_.note_closed(id);
    list_.close(id);
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(begin(100000).has_value());
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 10, 0)));
}

} // namespace
