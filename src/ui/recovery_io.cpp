// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// File work for the start-up question that must not block the main loop: a file's time, a copy.

#include "ui/recovery_io.h"

#include <gio/gio.h>

#include <memory>
#include <utility>

#include "core/recovery_answers.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

namespace {

// A file time query on its way: what to call with the answer.
struct TimeQuery {
    std::function<void(std::optional<std::int64_t>)> done;
};

// The end of a query started by query_file_time(); data is the TimeQuery it released.
void on_time(GObject* source, GAsyncResult* result, gpointer data) {
    std::unique_ptr<TimeQuery> query(static_cast<TimeQuery*>(data));
    GError* raw = nullptr;
    auto info =
        gtkx::ObjectPtr<GFileInfo>::adopt(g_file_query_info_finish(G_FILE(source), result, &raw));
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    std::optional<std::int64_t> mtime_us;
    if (info) {
        std::unique_ptr<GDateTime, decltype(&g_date_time_unref)> time(
            g_file_info_get_modification_date_time(info.get()), &g_date_time_unref);
        if (time) {
            mtime_us = g_date_time_to_unix_usec(time.get());
        }
    }
    query->done(mtime_us);
}

// One copy read on a worker thread: the store and id it started with, the callback and the
// result to hand back on the main loop.
struct ReadJob {
    ReadJob(RecoveryStore store, std::string id, std::function<void(CopyRead)> done)
        : store(std::move(store)), id(std::move(id)), done(std::move(done)) {}

    RecoveryStore store;
    std::string id;
    std::function<void(CopyRead)> done;
    CopyRead result;
};

void free_job(gpointer data) {
    delete static_cast<ReadJob*>(data);
}

// Runs on a worker thread: no widget, no GTK call, no global state.
void read_thread(GTask* task, gpointer /*source_object*/, gpointer task_data,
                 GCancellable* /*cancellable*/) {
    ReadJob* job = static_cast<ReadJob*>(task_data);
    auto read = job->store.read(job->id);
    if (!read.is_ok()) {
        job->result.problem = read.error().message;
    } else {
        RecoveryCopy copy = std::move(read).value();
        std::optional<std::string> problem = recovery_text_problem(copy.text);
        if (problem) {
            job->result.problem = *problem;
        } else {
            job->result.copy = std::move(copy);
        }
    }
    g_task_return_boolean(task, TRUE);
}

// Runs on the main loop, after read_thread: the task frees the job after this returns.
void on_copy(GObject* /*source_object*/, GAsyncResult* result, gpointer /*callback_data*/) {
    ReadJob* job = static_cast<ReadJob*>(g_task_get_task_data(G_TASK(result)));
    job->done(std::move(job->result));
}

} // namespace

void query_file_time(const std::string& path,
                     std::function<void(std::optional<std::int64_t>)> done) {
    auto query = std::make_unique<TimeQuery>();
    query->done = std::move(done);
    auto location = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    g_file_query_info_async(
        location.get(), G_FILE_ATTRIBUTE_TIME_MODIFIED "," G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
        G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, nullptr, &on_time, query.release());
}

void read_recovery_copy(const RecoveryStore& store, const std::string& id,
                        std::function<void(CopyRead)> done) {
    auto job = std::make_unique<ReadJob>(store, id, std::move(done));
    auto task = gtkx::ObjectPtr<GTask>::adopt(g_task_new(nullptr, nullptr, &on_copy, nullptr));
    g_task_set_task_data(task.get(), job.release(), free_job);
    g_task_run_in_thread(task.get(), read_thread);
}

} // namespace vermilion::ui
