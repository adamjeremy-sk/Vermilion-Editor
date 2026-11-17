// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the GLib debounce timer: it fires once, can be stopped or replaced, never outlives.

#include <glib.h>
#include <gtest/gtest.h>

#include <functional>

#include "ui/glib_timer.h"

namespace {

using vermilion::ui::GlibTimer;

// Runs the main loop for `milliseconds`.
void pump_for(int milliseconds) {
    const gint64 deadline = g_get_monotonic_time() + milliseconds * 1000;
    while (g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// Runs the main loop until `done` is true, for at most five seconds.
void pump_until(const std::function<bool()>& done) {
    const gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

TEST(GlibTimerTest, FiresOnceAfterTheDelay) {
    GlibTimer timer;
    int calls = 0;
    EXPECT_FALSE(timer.pending());
    timer.start(10, [&calls]() { ++calls; });
    EXPECT_TRUE(timer.pending());
    EXPECT_EQ(calls, 0);
    pump_until([&calls]() { return calls > 0; });
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(timer.pending());
    pump_for(60);
    EXPECT_EQ(calls, 1);
}

TEST(GlibTimerTest, StopDropsAPendingCall) {
    GlibTimer timer;
    int calls = 0;
    timer.start(10, [&calls]() { ++calls; });
    timer.stop();
    EXPECT_FALSE(timer.pending());
    pump_for(80);
    EXPECT_EQ(calls, 0);
    timer.stop();
    EXPECT_FALSE(timer.pending());
}

TEST(GlibTimerTest, AnotherStartReplacesTheCall) {
    GlibTimer timer;
    int first = 0;
    int second = 0;
    timer.start(10, [&first]() { ++first; });
    timer.start(10, [&second]() { ++second; });
    pump_until([&second]() { return second > 0; });
    pump_for(60);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(GlibTimerTest, ADestroyedTimerNeverFires) {
    int calls = 0;
    {
        GlibTimer timer;
        timer.start(10, [&calls]() { ++calls; });
    }
    pump_for(80);
    EXPECT_EQ(calls, 0);
}

TEST(GlibTimerTest, TheCallMayStartTheTimerAgain) {
    GlibTimer timer;
    int calls = 0;
    timer.start(10, [&timer, &calls]() {
        ++calls;
        timer.start(10, [&calls]() { ++calls; });
    });
    pump_until([&calls]() { return calls > 1; });
    EXPECT_EQ(calls, 2);
    EXPECT_FALSE(timer.pending());
}

} // namespace
