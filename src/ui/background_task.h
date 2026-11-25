// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs pure data work on a worker thread and hands the finished value to the main loop.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace vermilion::ui {

// How much the worker may take from the main thread. Normal runs the job on a GIO worker thread.
// Low runs it on a thread of its own, in the idle scheduling class (it gets the processor only
// when nothing else wants it); if the system refuses that, it runs as Normal on that thread.
enum class BackgroundPriority { Normal, Low };

// The question a job asks now and then, so that it can stop early: true once its scope is gone.
using CancelCheck = std::function<bool()>;

// Owned by the object that starts background jobs (a member, declared last so it goes first).
// Main thread only: run() and the destructor are called from the thread that runs the main loop.
// When the scope is destroyed every job that has not been handed back is cancelled: its `done`
// never runs, and a job that has not started yet may skip its work.
class BackgroundScope {
public:
    BackgroundScope();
    ~BackgroundScope();

    BackgroundScope(const BackgroundScope&) = delete;
    BackgroundScope& operator=(const BackgroundScope&) = delete;

    // Calls work(cancelled) on a worker thread, then done(result) once, on the main loop of the
    // thread that called run(), always later (never from inside run()). The work is pure data: no
    // widget, no GTK call, no state that the main thread also touches; it asks `cancelled()` at
    // convenient places and returns early when it is true. T may be a type that can only be
    // moved. The work must not throw (the program is built without exceptions).
    template <typename T>
    void run(std::function<T(const CancelCheck&)> work, std::function<void(T)> done,
             BackgroundPriority priority = BackgroundPriority::Low);

private:
    // The part of run() that does not depend on T: `work` and `deliver` are the two halves of
    // the job, `deliver` runs on the main loop unless the scope is gone by then.
    void start(BackgroundPriority priority, std::function<void()> work,
               std::function<void()> deliver);

    std::shared_ptr<std::atomic<bool>> stop_; // true once the scope is destroyed
};

template <typename T>
void BackgroundScope::run(std::function<T(const CancelCheck&)> work, std::function<void(T)> done,
                          BackgroundPriority priority) {
    auto result = std::make_shared<std::optional<T>>();
    std::shared_ptr<std::atomic<bool>> stop = stop_;
    start(
        priority,
        [work = std::move(work), result, stop]() {
            CancelCheck cancelled = [stop]() { return stop->load(); };
            result->emplace(work(cancelled));
        },
        [done = std::move(done), result]() { done(std::move(**result)); });
}

} // namespace vermilion::ui
