// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs a job on a worker thread; the hand-back is on the main loop.

#include "ui/background_task.h"

#include <gio/gio.h>
#include <pthread.h>
#include <sched.h>

#include <memory>
#include <utility>

namespace vermilion::ui {

namespace {

// One job on its way: the scope's flag, the priority, the two halves of the job.
struct Job {
    std::shared_ptr<std::atomic<bool>> stop;
    BackgroundPriority priority = BackgroundPriority::Normal;
    std::function<void()> work;
    std::function<void()> deliver;
};

void free_job(gpointer data) {
    delete static_cast<Job*>(data);
}

// Puts the calling thread in the idle scheduling class, for the rest of its life. There is no way
// back for an ordinary user (a thread may leave the idle class only if its nice value is within
// RLIMIT_NICE, which is 0 on most desktops), so this is done only on a thread of the job's own,
// never on a worker thread that GIO shares with every other asynchronous call.
void enter_idle_class() {
    sched_param idle{};
    pthread_setschedparam(pthread_self(), SCHED_IDLE, &idle); // refused: the job runs as Normal
}

// The work of a job, on a worker thread: no widget, no GTK call, no global state. A job whose
// scope is already gone does not start.
void work_of(Job& job) {
    if (!job.stop->load()) {
        job.work();
    }
}

// Runs on a GIO worker thread (a Normal job).
void run_job(GTask* task, gpointer /*source_object*/, gpointer task_data,
             GCancellable* /*cancellable*/) {
    work_of(*static_cast<Job*>(task_data));
    g_task_return_boolean(task, TRUE);
}

// Runs on the thread of a Low job, which ends with the job. `data` is the task, whose reference
// this thread owns.
gpointer run_low_job(gpointer data) {
    GTask* task = G_TASK(data);
    enter_idle_class();
    work_of(*static_cast<Job*>(g_task_get_task_data(task)));
    g_task_return_boolean(task, TRUE); // the callback runs on the main loop that made the task
    g_object_unref(task);
    return nullptr;
}

// Runs on the main loop, after the work: the task frees the job after this returns.
void finished(GObject* /*source_object*/, GAsyncResult* result, gpointer /*callback_data*/) {
    Job* job = static_cast<Job*>(g_task_get_task_data(G_TASK(result)));
    if (!job->stop->load()) {
        job->deliver();
    }
}

} // namespace

BackgroundScope::BackgroundScope() : stop_(std::make_shared<std::atomic<bool>>(false)) {}

BackgroundScope::~BackgroundScope() {
    stop_->store(true);
}

void BackgroundScope::start(BackgroundPriority priority, std::function<void()> work,
                            std::function<void()> deliver) {
    auto job = std::make_unique<Job>(Job{stop_, priority, std::move(work), std::move(deliver)});
    GTask* task = g_task_new(nullptr, nullptr, finished, nullptr);
    g_task_set_task_data(task, job.release(), free_job); // the task frees the job
    if (priority == BackgroundPriority::Low) {
        GThread* thread = g_thread_new("vermilion-low", run_low_job, task); // owns the reference
        g_thread_unref(thread); // never joined: the thread ends with its job
        return;
    }
    g_task_run_in_thread(task, run_job);
    g_object_unref(task); // the task keeps itself alive until finished() has run
}

} // namespace vermilion::ui
