// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that HeldEvents holds events back, remembers each kind, and hands them over once.

#include <gtest/gtest.h>

#include "core/held_events.h"

namespace {

using vermilion::HeldEvents;

TEST(HeldEventsTest, WithoutAHoldNothingIsHeldBack) {
    HeldEvents events;
    EXPECT_FALSE(events.holding());
    EXPECT_FALSE(events.hold_edited());
    EXPECT_FALSE(events.hold_cursor());
    const HeldEvents::Pending pending = events.release();
    EXPECT_FALSE(pending.edited);
    EXPECT_FALSE(pending.cursor);
}

TEST(HeldEventsTest, AHoldHoldsBackBothKinds) {
    HeldEvents events;
    events.hold();
    EXPECT_TRUE(events.holding());
    EXPECT_TRUE(events.hold_edited());
    EXPECT_TRUE(events.hold_cursor());
}

TEST(HeldEventsTest, ReleaseSaysAnEditCameIn) {
    HeldEvents events;
    events.hold();
    events.hold_edited();
    const HeldEvents::Pending pending = events.release();
    EXPECT_TRUE(pending.edited);
    EXPECT_FALSE(pending.cursor);
}

TEST(HeldEventsTest, ReleaseSaysACursorMoveCameIn) {
    HeldEvents events;
    events.hold();
    events.hold_cursor();
    const HeldEvents::Pending pending = events.release();
    EXPECT_FALSE(pending.edited);
    EXPECT_TRUE(pending.cursor);
}

TEST(HeldEventsTest, ManyEventsOfOneKindAreHandedOverOnce) {
    HeldEvents events;
    events.hold();
    for (int i = 0; i < 1000; ++i) {
        events.hold_edited();
        events.hold_cursor();
    }
    const HeldEvents::Pending first = events.release();
    EXPECT_TRUE(first.edited);
    EXPECT_TRUE(first.cursor);
    const HeldEvents::Pending second = events.release();
    EXPECT_FALSE(second.edited);
    EXPECT_FALSE(second.cursor);
}

TEST(HeldEventsTest, ReleaseEndsTheHold) {
    HeldEvents events;
    events.hold();
    events.release();
    EXPECT_FALSE(events.holding());
    EXPECT_FALSE(events.hold_edited());
    EXPECT_FALSE(events.hold_cursor());
}

TEST(HeldEventsTest, HoldingAgainKeepsWhatWasRemembered) {
    HeldEvents events;
    events.hold();
    events.hold_edited();
    events.hold();
    const HeldEvents::Pending pending = events.release();
    EXPECT_TRUE(pending.edited);
    EXPECT_FALSE(pending.cursor);
}

TEST(HeldEventsTest, ANewHoldStartsWithNothingRemembered) {
    HeldEvents events;
    events.hold();
    events.hold_edited();
    events.hold_cursor();
    events.release();
    events.hold();
    const HeldEvents::Pending pending = events.release();
    EXPECT_FALSE(pending.edited);
    EXPECT_FALSE(pending.cursor);
}

TEST(HeldEventsTest, EventsAfterTheReleaseAreNotRemembered) {
    HeldEvents events;
    events.hold();
    events.release();
    events.hold_edited();
    events.hold();
    const HeldEvents::Pending pending = events.release();
    EXPECT_FALSE(pending.edited);
}

TEST(HeldEventsTest, ReleaseWithoutAHoldChangesNothing) {
    HeldEvents events;
    events.release();
    events.release();
    EXPECT_FALSE(events.holding());
    events.hold();
    EXPECT_TRUE(events.holding());
}

} // namespace
