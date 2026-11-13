// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Crash recovery while the program runs: copies edited documents on a timer, files off the main
// loop.

#pragma once

#include <glib.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "core/document_list.h"
#include "core/recovery_store.h"
#include "core/recovery_tracker.h"
#include "ui/document_observer.h"
#include "ui/gtkx/async_guard.h"

namespace vermilion::ui {

// Listens to the document tabs (DocumentObserver) and keeps one recovery copy per edited document
// (ARCHITECTURE.md 7.11). The decisions are the RecoveryTracker's; this class is the plumbing: a
// timer on the main loop that is armed only while a copy is pending, one worker thread that
// writes and deletes the files in order, and the status messages. It uses GLib and the core only,
// never GTK: the text of a document comes through a callback.
//
// Taking the text is the only part that holds the main loop; its time is logged for every copy
// (G_MESSAGES_DEBUG=vermilion) and handed to the tracker, which backs off in proportion.
class RecoveryController : public DocumentObserver {
public:
    struct Callbacks {
        // The text of the document exactly as its buffer holds it, or nothing when it has no page.
        // Called from the main loop, from the timer, once per copy.
        std::function<std::optional<std::string>(DocumentId)> take_text;
        // A line for the status bar's message panel; called from the main loop.
        std::function<void(const std::string&)> message;
    };

    // list is borrowed and must outlive this object. root is the folder that holds one subfolder
    // per program (recovery_root of the state folder; tests pass a temporary one). Leftover
    // temporary files of an earlier run are deleted here. When there is no usable folder or no
    // worker thread, active() is false and every call does nothing.
    RecoveryController(const DocumentList& list, const std::string& root, RecoveryProgram program,
                       Callbacks callbacks);

    // Stops the timer, waits until the worker has done every write and delete it was given, and
    // then deletes the copy of every document still open: this is a clean end of the program, so
    // every document has been answered (saved, or closed without saving). A crash never gets here.
    ~RecoveryController() override;

    RecoveryController(const RecoveryController&) = delete;
    RecoveryController& operator=(const RecoveryController&) = delete;

    bool active() const { return pool_ != nullptr; }

    // The document was recovered from the copy with this id (the start-up question): the
    // controller takes the id over, so the document's next copy replaces that file, and the file
    // is deleted when the document is saved, closed without saving, or at a clean end of the
    // program. file_mtime_us is the file time the copy's text was based on (its header). Call it
    // from the main loop, after the document is in the list. True when the id was taken over;
    // false, and nothing changes, when the controller is not active or RecoveryTracker::adopt_copy
    // refuses (an unknown document, an invalid id, a document that already has an id, an id that
    // another document uses): the copy then stays on disk and is offered again at the next start.
    bool adopt(DocumentId id, const std::string& recovery_id,
               std::optional<std::int64_t> file_mtime_us);

    void document_edited(DocumentId id, std::uint64_t chars) override;
    void document_modified(DocumentId id, bool modified) override;
    void document_saved(DocumentId id) override;
    void document_reloading(DocumentId id) override;
    void document_synced(DocumentId id, std::optional<std::int64_t> file_mtime_us) override;
    void document_closing(DocumentId id) override;

private:
    struct Task;

    struct PoolDeleter {
        // Waits for the queued tasks; nothing is dropped.
        void operator()(GThreadPool* pool) const { g_thread_pool_free(pool, FALSE, TRUE); }
    };

    void clean(DocumentId id);
    void apply(const RecoveryEffects& effects);
    void arm();
    void disarm();
    void on_wake();
    void start_copy(RecoveryJob job);
    void push(std::unique_ptr<Task> task);
    void finish(const Task& task);

    static gboolean on_timer(gpointer ticket);
    static void run_task(gpointer data, gpointer store);
    static void on_task_done(gpointer data);

    Callbacks callbacks_;
    RecoveryStore store_;
    RecoveryTracker tracker_;
    bool failure_shown_ = false; // a failure message is on the status bar; no copy written since
    std::unique_ptr<GThreadPool, PoolDeleter> pool_;   // after store_: it is freed first
    guint timer_ = 0;                                  // the main loop source, 0 when not armed
    std::optional<Millis> armed_for_;                  // the wake_at() the timer was set for
    gtkx::AsyncGuard<RecoveryController> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
