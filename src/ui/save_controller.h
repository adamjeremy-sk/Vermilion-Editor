// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Save, Save As, and the questions before a tab or the window closes with unsaved changes.

#pragma once

#include <gtk/gtk.h>

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/external_change.h"
#include "ui/document_saver.h"
#include "ui/document_tabs.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

class SaveController {
public:
    struct Callbacks {
        std::function<void()> changed;                   // a save started or ended
        std::function<void(const std::string&)> message; // a line for the status bar
        // An error for a dialog: message and detail.
        std::function<void(const std::string&, const std::string&)> error;
        // A read-only look at the document's file (ExternalChangeController::look): calls done
        // once with what it found, or never when the window is going away.
        std::function<void(DocumentId, std::function<void(FileLook)>)> look;
    };

    // parent, list and tabs are borrowed and must outlive this object.
    SaveController(GtkWindow* parent, DocumentList& list, DocumentTabs& tabs, Callbacks callbacks);
    ~SaveController();

    SaveController(const SaveController&) = delete;
    SaveController& operator=(const SaveController&) = delete;

    // File > Save: Save As for an untitled document. done (optional) gets true once saved.
    void save(DocumentId id, std::function<void(bool)> done = {});

    // File > Save As: asks for a name, then saves there.
    void save_as(DocumentId id, std::function<void(bool)> done = {});

    // Closing a tab: looks at the document's file first, then closes a document that needs no
    // save at once, or asks Save / Close Without Saving / Cancel for one with unsaved changes or a
    // file gone from disk (the wording follows close_case()).
    void request_close(DocumentId id);

    // Quitting: looks at the file of every document first; then asks about each document that
    // needs a save (unsaved changes or a file gone from disk) in turn, selecting its tab; Save
    // and Close Without Saving go on to the next, Cancel (or a failed save) stops. When none is
    // left, quit runs from the main loop.
    void ask_before_quit(std::function<void()> quit);

    // Called by DocumentTabs when a save ended (wired in MainWindow).
    void on_saved(DocumentId id, DocumentSaver::Outcome outcome, const std::string& error);

private:
    struct Pending {
        std::string path;
        TextFormat format;
        std::function<void(bool)> done;
    };

    void start(DocumentId id, const std::string& path, bool overwrite_changes,
               std::function<void(bool)> done);
    void finish(DocumentId id, DocumentSaver::Outcome outcome, const std::string& error);
    void look_at(DocumentId id, std::function<void(FileLook)> done);
    void continue_close(DocumentId id, FileLook look);
    void quit_looked(DocumentId id, FileLook look);
    void next_quit_question();
    void defer(std::function<void()> work);
    static void on_chosen(GObject* source, GAsyncResult* result, gpointer ticket);
    static void run_deferred(gpointer ticket);

    GtkWindow* parent_; // borrowed
    DocumentList& list_;
    DocumentTabs& tabs_;
    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_; // cancels an open Save As dialog
    std::map<DocumentId, Pending> pending_;     // saves in progress
    DocumentId choosing_ = 0;                   // the document the Save As dialog is for
    std::function<void(bool)> choosing_done_;
    bool quitting_ = false;
    std::vector<DocumentId> quit_skipped_;      // answered Close Without Saving while quitting
    std::map<DocumentId, FileLook> quit_looks_; // what the look at each file found, at quit
    std::size_t quit_waiting_ = 0;              // looks at quit that have not answered yet
    std::set<DocumentId> closing_;              // tabs whose close waits for their look
    std::function<void()> quit_;
    std::vector<std::function<void()>> deferred_;
    bool idle_pending_ = false;
    gtkx::AsyncGuard<SaveController> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
