// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for BackgroundScope: work on a worker thread, the value handed back on the main loop.

#include <glib.h>
#include <gtest/gtest.h>
#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ui/background_task.h"

namespace {

using vermilion::ui::BackgroundPriority;
using vermilion::ui::BackgroundScope;
using vermilion::ui::CancelCheck;

// Runs the main loop until `done` is true, for at most ten seconds.
void pump_until(const std::function<bool()>& done) {
    const gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// Runs the main loop for `milliseconds`.
void pump_for(int milliseconds) {
    const gint64 deadline = g_get_monotonic_time() + milliseconds * 1000;
    while (g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// True when this system lets a thread of the test program enter the idle scheduling class.
bool idle_class_allowed() {
    std::atomic<bool> allowed{false};
    std::thread probe([&allowed]() {
        sched_param idle{};
        allowed = pthread_setschedparam(pthread_self(), SCHED_IDLE, &idle) == 0;
    });
    probe.join();
    return allowed;
}

TEST(BackgroundTaskTest, TheWorkRunsOffTheMainThreadAndTheValueComesBackOnIt) {
    BackgroundScope scope;
    const std::thread::id main_thread = std::this_thread::get_id();
    std::thread::id worked_on;
    std::thread::id handed_back_on;
    int calls = 0;
    std::string value;
    scope.run<std::string>(
        [&worked_on](const CancelCheck&) {
            worked_on = std::this_thread::get_id();
            return std::string("parsed");
        },
        [&](std::string result) {
            handed_back_on = std::this_thread::get_id();
            value = std::move(result);
            ++calls;
        });
    EXPECT_EQ(calls, 0) << "done must not run inside run()";
    pump_until([&calls]() { return calls > 0; });
    pump_for(20);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(value, "parsed");
    EXPECT_NE(worked_on, main_thread);
    EXPECT_EQ(handed_back_on, main_thread);
}

TEST(BackgroundTaskTest, AValueThatCanOnlyBeMovedComesBackWithoutACopy) {
    BackgroundScope scope;
    std::unique_ptr<int> value;
    scope.run<std::unique_ptr<int>>(
        [](const CancelCheck&) { return std::make_unique<int>(42); },
        [&value](std::unique_ptr<int> result) { value = std::move(result); });
    pump_until([&value]() { return value != nullptr; });
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
}

TEST(BackgroundTaskTest, EveryJobIsHandedBackExactlyOnce) {
    BackgroundScope scope;
    std::vector<int> seen(12, 0);
    int total = 0;
    for (int i = 0; i < 12; ++i) {
        scope.run<int>([i](const CancelCheck&) { return i; },
                       [&seen, &total](int result) {
                           ++seen[static_cast<std::size_t>(result)];
                           ++total;
                       });
    }
    pump_until([&total]() { return total >= 12; });
    pump_for(20);
    EXPECT_EQ(total, 12);
    for (int count : seen) {
        EXPECT_EQ(count, 1);
    }
}

TEST(BackgroundTaskTest, ADestroyedScopeCancelsTheJobAndNothingIsHandedBack) {
    auto scope = std::make_unique<BackgroundScope>();
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> saw_cancel{false};
    int calls = 0;
    scope->run<int>(
        [&](const CancelCheck& cancelled) {
            started = true;
            while (!release) {
                g_usleep(500);
            }
            saw_cancel = cancelled();
            finished = true;
            return 1;
        },
        [&calls](int) { ++calls; });
    pump_until([&started]() { return started.load(); });
    ASSERT_TRUE(started);
    scope.reset();
    release = true;
    pump_until([&finished]() { return finished.load(); });
    pump_for(50);
    EXPECT_TRUE(finished);
    EXPECT_TRUE(saw_cancel) << "the job must be able to see that its scope is gone";
    EXPECT_EQ(calls, 0) << "nothing is handed back to a scope that is gone";
}

TEST(BackgroundTaskTest, AScopeDestroyedRightAfterRunHandsNothingBack) {
    // Whether the job has started by then depends on the pool, so only what must hold either way
    // is checked: the answer is never handed back.
    int calls = 0;
    {
        BackgroundScope scope;
        scope.run<int>([](const CancelCheck&) { return 1; }, [&calls](int) { ++calls; });
    }
    pump_for(100);
    EXPECT_EQ(calls, 0);
}

TEST(BackgroundTaskTest, ALowPriorityJobRunsInTheIdleClassWhereTheSystemAllowsIt) {
    const bool allowed = idle_class_allowed();
    BackgroundScope scope;
    int policy = -1;
    bool done = false;
    scope.run<int>([](const CancelCheck&) { return sched_getscheduler(0); },
                   [&](int result) {
                       policy = result;
                       done = true;
                   },
                   BackgroundPriority::Low);
    pump_until([&done]() { return done; });
    ASSERT_TRUE(done);
    if (allowed) {
        EXPECT_EQ(policy, SCHED_IDLE);
    } else {
        EXPECT_EQ(policy, SCHED_OTHER) << "refused by the system: the job runs as Normal";
    }
}

TEST(BackgroundTaskTest, ANormalJobKeepsTheDefaultClassAndALowJobLeavesNoThreadLowered) {
    BackgroundScope scope;
    std::vector<int> policies;
    int done_count = 0;
    for (int i = 0; i < 8; ++i) {
        scope.run<int>([](const CancelCheck&) { return 0; }, [&done_count](int) { ++done_count; },
                       BackgroundPriority::Low);
    }
    pump_until([&done_count]() { return done_count == 8; });
    for (int i = 0; i < 16; ++i) {
        scope.run<int>([](const CancelCheck&) { return sched_getscheduler(0); },
                       [&](int result) {
                           policies.push_back(result);
                           ++done_count;
                       },
                       BackgroundPriority::Normal);
    }
    pump_until([&done_count]() { return done_count == 24; });
    ASSERT_EQ(policies.size(), 16u);
    for (int policy : policies) {
        EXPECT_NE(policy, SCHED_IDLE);
    }
}

} // namespace
