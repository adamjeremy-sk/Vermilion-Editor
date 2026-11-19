// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for OpenQueue: order, one file at a time, pushing while busy, clearing.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "core/open_queue.h"

namespace {

using vermilion::OpenQueue;
using Next = std::optional<std::string>;

TEST(OpenQueueTest, AFreshQueueIsIdleAndEmpty) {
    OpenQueue queue;
    EXPECT_FALSE(queue.busy());
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_FALSE(queue.next().has_value());
}

TEST(OpenQueueTest, PushStartsNothing) {
    OpenQueue queue;
    queue.push({"/a", "/b"});
    EXPECT_FALSE(queue.busy());
    EXPECT_EQ(queue.pending(), 2u);
}

TEST(OpenQueueTest, PathsComeOutInOrderOneAtATime) {
    OpenQueue queue;
    queue.push({"/a", "/b", "/c"});
    EXPECT_EQ(queue.next(), Next("/a"));
    EXPECT_TRUE(queue.busy());
    EXPECT_EQ(queue.pending(), 2u);
    EXPECT_FALSE(queue.next().has_value());
    EXPECT_EQ(queue.pending(), 2u);
    queue.finished();
    EXPECT_FALSE(queue.busy());
    EXPECT_EQ(queue.next(), Next("/b"));
    queue.finished();
    EXPECT_EQ(queue.next(), Next("/c"));
    queue.finished();
    EXPECT_FALSE(queue.next().has_value());
    EXPECT_FALSE(queue.busy());
}

TEST(OpenQueueTest, PushWhileBusyAppends) {
    OpenQueue queue;
    queue.push({"/a"});
    EXPECT_EQ(queue.next(), Next("/a"));
    queue.push({"/b", "/c"});
    EXPECT_EQ(queue.pending(), 2u);
    EXPECT_FALSE(queue.next().has_value());
    queue.finished();
    EXPECT_EQ(queue.next(), Next("/b"));
}

TEST(OpenQueueTest, PushOfNothingChangesNothing) {
    OpenQueue queue;
    queue.push({});
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_FALSE(queue.busy());
}

TEST(OpenQueueTest, FinishedWhileIdleDoesNothing) {
    OpenQueue queue;
    queue.finished();
    queue.push({"/a"});
    queue.finished();
    EXPECT_EQ(queue.pending(), 1u);
    EXPECT_EQ(queue.next(), Next("/a"));
}

TEST(OpenQueueTest, ClearForgetsTheWaitingPaths) {
    OpenQueue queue;
    queue.push({"/a", "/b", "/c"});
    queue.clear();
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_FALSE(queue.next().has_value());
}

TEST(OpenQueueTest, ClearLeavesTheRunningFileBusy) {
    OpenQueue queue;
    queue.push({"/a", "/b"});
    EXPECT_EQ(queue.next(), Next("/a"));
    queue.clear();
    EXPECT_TRUE(queue.busy());
    EXPECT_EQ(queue.pending(), 0u);
    queue.finished();
    EXPECT_FALSE(queue.busy());
    EXPECT_FALSE(queue.next().has_value());
}

TEST(OpenQueueTest, TheSamePathCanBeQueuedTwice) {
    OpenQueue queue;
    queue.push({"/a", "/a"});
    EXPECT_EQ(queue.next(), Next("/a"));
    queue.finished();
    EXPECT_EQ(queue.next(), Next("/a"));
}

} // namespace
