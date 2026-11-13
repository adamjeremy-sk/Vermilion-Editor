// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The recovery timer, the worker thread that writes the copies, and what the tracker asks for.

#include "ui/recovery_controller.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "core/log.h"

namespace vermilion::ui {

namespace {

constexpr const char* kTooLargeMessage = "Too large for recovery copies";
constexpr const char* kFailedMessage = "Could not write a recovery copy.";
constexpr const char* kWorkingAgainMessage = "Recovery copies are working again.";

// The monotonic clock in milliseconds: the time the schedule is given.
Millis now_ms() {
    return g_get_monotonic_time() / 1000;
}

} // namespace

// One job for the worker: write a copy, or delete one. It goes to the worker thread and comes
// back to the main loop with the outcome filled in.
struct RecoveryController::Task {
    enum class Kind { Write, Remove };

    Kind kind = Kind::Write;
    std::string id;             // the recovery id
    RecoveryHeader header;      // Write only
    std::string text;           // Write only; freed by the worker as soon as it is written
    DocumentId document = 0;    // Write only
    Millis freeze_ms = 0;       // Write only: how long taking the text held the main loop
    std::uint64_t bytes = 0;    // Write only
    std::int64_t worker_us = 0; // the time the worker spent on the file
    bool ok = false;            // the worker's result
    std::string error;          // when not ok
    gpointer ticket = nullptr;  // from guard_; it says whether the controller still exists
};

RecoveryController::RecoveryController(const DocumentList& list, const std::string& root,
                                       RecoveryProgram program, Callbacks callbacks)
    : callbacks_(std::move(callbacks)), store_(root, program), tracker_(list) {
    if (store_.folder().empty()) {
        logging::warning("recovery: no usable state folder, crash recovery is off");
        return;
    }
    std::size_t removed = store_.remove_leftovers();
    if (removed > 0) {
        logging::debug("recovery: deleted " + std::to_string(removed) +
                       " leftover temporary files");
    }
    GError* raw = nullptr;
    GThreadPool* pool = g_thread_pool_new(&RecoveryController::run_task, &store_, 1, TRUE, &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (pool == nullptr) {
        logging::warning("recovery: no worker thread, crash recovery is off");
        return;
    }
    pool_.reset(pool);
}

RecoveryController::~RecoveryController() {
    disarm();
    pool_.reset();
    for (const std::string& id : tracker_.copy_ids()) {
        Status status = store_.remove(id);
        if (!status.is_ok()) {
            logging::warning("recovery: could not delete a copy: " + status.error().message);
        }
    }
}

bool RecoveryController::adopt(DocumentId id, const std::string& recovery_id,
                               std::optional<std::int64_t> file_mtime_us) {
    return active() && tracker_.adopt_copy(id, recovery_id, file_mtime_us);
}

void RecoveryController::document_edited(DocumentId id, std::uint64_t chars) {
    if (!active()) {
        return;
    }
    apply(tracker_.note_edit(id, chars, now_ms()));
    arm();
}

// The flag turning on says nothing the edit did not already say (the buffer reports the change
// before the flag); only the flag turning off, a save or an Undo back to the saved text, matters.
void RecoveryController::document_modified(DocumentId id, bool modified) {
    if (!modified) {
        clean(id);
    }
}

void RecoveryController::document_saved(DocumentId id) {
    clean(id);
}

void RecoveryController::document_reloading(DocumentId id) {
    clean(id);
}

void RecoveryController::document_synced(DocumentId id, std::optional<std::int64_t> file_mtime_us) {
    if (active()) {
        tracker_.set_file_mtime(id, file_mtime_us);
    }
}

void RecoveryController::document_closing(DocumentId id) {
    if (!active()) {
        return;
    }
    apply(tracker_.note_closed(id));
    arm();
}

void RecoveryController::clean(DocumentId id) {
    if (!active()) {
        return;
    }
    apply(tracker_.note_clean(id));
    arm();
}

void RecoveryController::apply(const RecoveryEffects& effects) {
    if (effects.delete_id) {
        auto task = std::make_unique<Task>();
        task->kind = Task::Kind::Remove;
        task->id = *effects.delete_id;
        push(std::move(task));
    }
    if (effects.announce_too_large) {
        callbacks_.message(kTooLargeMessage);
    }
    if (effects.announce_failure) {
        callbacks_.message(kFailedMessage);
        failure_shown_ = true;
    }
}

// The timer exists only while the tracker has something pending, and is moved only when the time
// it wants has changed (every keystroke comes through here).
void RecoveryController::arm() {
    std::optional<Millis> wake = tracker_.wake_at();
    if (!wake) {
        disarm();
        return;
    }
    if (timer_ != 0 && armed_for_ == wake) {
        return;
    }
    disarm();
    Millis delay = std::max<Millis>(*wake - now_ms(), 0);
    guint ms = static_cast<guint>(std::min<Millis>(delay, std::numeric_limits<guint>::max()));
    timer_ =
        g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, ms, &RecoveryController::on_timer,
                           guard_.ticket(), &gtkx::AsyncGuard<RecoveryController>::free_ticket);
    armed_for_ = wake;
}

void RecoveryController::disarm() {
    if (timer_ != 0) {
        g_source_remove(timer_);
        timer_ = 0;
    }
    armed_for_.reset();
}

gboolean RecoveryController::on_timer(gpointer ticket) {
    RecoveryController* self = gtkx::AsyncGuard<RecoveryController>::peek(ticket);
    if (self != nullptr) {
        self->timer_ = 0;
        self->armed_for_.reset();
        self->on_wake();
    }
    return G_SOURCE_REMOVE;
}

// The timer fired: copy the document that is due, if one is (the timer may be a millisecond
// early), and set the timer again.
void RecoveryController::on_wake() {
    std::optional<RecoveryJob> job = tracker_.begin_next(now_ms(), g_get_real_time());
    if (job) {
        start_copy(std::move(*job));
    }
    arm();
}

// Takes the text, which is the one step that holds the main loop, and hands the write to the
// worker.
void RecoveryController::start_copy(RecoveryJob job) {
    gint64 started = g_get_monotonic_time();
    std::optional<std::string> text = callbacks_.take_text(job.document);
    Millis freeze_ms = (g_get_monotonic_time() - started) / 1000;
    if (!text) {
        // Every open document has a page, so this does not happen; ending the copy keeps the
        // schedule from stalling if it ever does.
        apply(tracker_.end_copy(now_ms(), 0, true));
        return;
    }
    auto task = std::make_unique<Task>();
    task->kind = Task::Kind::Write;
    task->id = job.recovery_id;
    task->header = job.header;
    task->document = job.document;
    task->freeze_ms = freeze_ms;
    task->bytes = text->size();
    task->text = std::move(*text);
    push(std::move(task));
}

// A task that the worker cannot be given is finished at once as a failure.
void RecoveryController::push(std::unique_ptr<Task> task) {
    task->ticket = guard_.ticket();
    GError* raw = nullptr;
    if (g_thread_pool_push(pool_.get(), task.get(), &raw)) {
        task.release();
        return;
    }
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    task->ok = false;
    task->error = error ? error->message : "the worker did not take the task";
    gtkx::AsyncGuard<RecoveryController>::free_ticket(task->ticket);
    finish(*task);
}

// Runs on the worker thread: the file, then back to the main loop. The text is freed here, so the
// main loop never frees a large one.
void RecoveryController::run_task(gpointer data, gpointer store) {
    std::unique_ptr<Task> task(static_cast<Task*>(data));
    const RecoveryStore* files = static_cast<const RecoveryStore*>(store);
    gint64 started = g_get_monotonic_time();
    Status status = task->kind == Task::Kind::Write
                        ? files->write(task->id, task->header, task->text)
                        : files->remove(task->id);
    task->worker_us = g_get_monotonic_time() - started;
    task->ok = status.is_ok();
    if (!task->ok) {
        task->error = status.error().message;
    }
    std::string().swap(task->text);
    g_idle_add_once(&RecoveryController::on_task_done, task.release());
}

void RecoveryController::on_task_done(gpointer data) {
    std::unique_ptr<Task> task(static_cast<Task*>(data));
    RecoveryController* self = gtkx::AsyncGuard<RecoveryController>::take(task->ticket);
    if (self != nullptr) {
        self->finish(*task);
    }
}

// Back on the main loop with the outcome of one task. A copy that was written or failed is
// logged (its hold is how the size ceiling is settled), told to the tracker, and the timer is set
// again; a failed delete is only logged. The first copy written after a failure was announced
// replaces that message with a note that copies work again.
void RecoveryController::finish(const Task& task) {
    if (task.kind == Task::Kind::Remove) {
        if (!task.ok) {
            logging::warning("recovery: could not delete a copy: " + task.error);
        }
        return;
    }
    std::string line = "recovery: copy of document " + std::to_string(task.document) + ", " +
                       std::to_string(task.bytes) + " bytes, main loop held " +
                       std::to_string(task.freeze_ms) + " ms, written in " +
                       std::to_string(task.worker_us / 1000) + " ms";
    if (task.ok) {
        logging::debug(line);
    } else {
        logging::warning(line + ", failed: " + task.error);
    }
    apply(tracker_.end_copy(now_ms(), task.freeze_ms, task.ok));
    if (task.ok && failure_shown_) {
        failure_shown_ = false;
        callbacks_.message(kWorkingAgainMessage);
    }
    arm();
}

} // namespace vermilion::ui
