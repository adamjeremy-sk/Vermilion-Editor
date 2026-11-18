// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reads a file's state and hash on a GIO worker thread.

#include "ui/file_inspector.h"

#include <gio/gio.h>
#include <memory>
#include <utility>

#include "core/file_hash.h"

namespace vermilion::ui {

namespace {

// One inspection on its way: the file looked at, whether its bytes were hashed, the callback and
// the result to hand back on the main loop.
struct InspectJob {
    std::string path;
    bool with_hash = false;
    InspectDone done;
    FileInspection result;
};

void free_job(gpointer data) {
    delete static_cast<InspectJob*>(data);
}

// Runs on a worker thread: no widget, no GTK call, no global state.
void run(GTask* task, gpointer /*source_object*/, gpointer task_data,
         GCancellable* /*cancellable*/) {
    InspectJob* job = static_cast<InspectJob*>(task_data);
    GError* raw = nullptr;
    GFile* file = g_file_new_for_path(job->path.c_str());
    GFileInfo* info =
        g_file_query_info(file,
                          G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_TIME_MODIFIED
                                                         "," G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
                          G_FILE_QUERY_INFO_NONE, nullptr, &raw);
    if (info != nullptr) {
        job->result.readable = true;
        job->result.disk.exists = true;
        job->result.disk.size = g_file_info_get_size(info);
        GDateTime* time = g_file_info_get_modification_date_time(info); // new reference
        if (time != nullptr) {
            job->result.disk.mtime_us = g_date_time_to_unix_usec(time);
            g_date_time_unref(time);
        }
        if (job->with_hash) {
            Result<FileHash> hashed = hash_file_blocking(job->path);
            if (hashed.is_ok() && hashed.value().size == job->result.disk.size) {
                job->result.hash = hashed.value().hash;
            }
        }
    } else if (g_error_matches(raw, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
        job->result.readable = true;
        // exists stays false; the time and size stay zero.
    } else {
        job->result.readable = false;
    }
    g_clear_error(&raw);
    g_clear_object(&info);
    g_clear_object(&file);
    g_task_return_boolean(task, TRUE);
}

// Runs on the main loop, after run(): the task frees the job after this returns.
void finished(GObject* /*source_object*/, GAsyncResult* result, gpointer /*callback_data*/) {
    InspectJob* job = static_cast<InspectJob*>(g_task_get_task_data(G_TASK(result)));
    InspectDone done = std::move(job->done);
    done(job->result);
}

} // namespace

void inspect_file(const std::string& path, bool with_hash, InspectDone done) {
    InspectJob* job = new InspectJob{path, with_hash, std::move(done), FileInspection{}};
    GTask* task = g_task_new(nullptr, nullptr, finished, nullptr);
    g_task_set_task_data(task, job, free_job);
    g_task_run_in_thread(task, run);
    g_object_unref(task); // the task keeps itself alive until finished() has run
}

} // namespace vermilion::ui
