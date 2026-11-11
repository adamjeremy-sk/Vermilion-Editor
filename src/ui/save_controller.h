// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Save, Save As, and the questions before a tab or the window closes with unsaved changes.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/document_list.h"
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

    // Closing a tab: Save / Close Without Saving / Cancel for a document with unsaved changes.
    void request_close(DocumentId id);

    // Quitting: asks about each document with unsaved changes in turn, selecting its tab; Save
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
    std::vector<DocumentId> quit_skipped_; // answered Close Without Saving while quitting
    std::function<void()> quit_;
    std::vector<std::function<void()>> deferred_;
    bool idle_pending_ = false;
    gtkx::AsyncGuard<SaveController> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
