// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Document tabs: a notebook with one editor page per DocumentList record, kept in the same order.

#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/text_scan.h"
#include "ui/document_loader.h"
#include "ui/document_observer.h"
#include "ui/editor_page.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/status_bar.h"

namespace vermilion::ui {

class DocumentTabs {
public:
    struct Callbacks {
        // The documents, the active tab, a modified flag or the active cursor changed.
        std::function<void()> changed;
        // The user asked to close a tab (close button or middle click); runs from an idle
        // callback, never inside the click itself.
        std::function<void(DocumentId)> close_requested;
        // A line for the status bar's message panel (load progress and results).
        std::function<void(const std::string&)> message;
        // A file could not be loaded; its tab is already closed. Message and detail for a dialog.
        std::function<void(const std::string&, const std::string&)> load_failed;
        // A save of document id ended (the error text when Failed). Runs inside the save's own
        // callback: the receiver must not close the tab from there.
        std::function<void(DocumentId, DocumentSaver::Outcome, const std::string&)> saved;
    };

    // list must outlive this object.
    DocumentTabs(DocumentList& list, Callbacks callbacks);
    ~DocumentTabs();

    DocumentTabs(const DocumentTabs&) = delete;
    DocumentTabs& operator=(const DocumentTabs&) = delete;

    GtkWidget* widget() const { return box_.get(); }

    // Sets the listener for the events of document_observer.h, or none (nullptr). Borrowed: it
    // must outlive this object, or be removed first.
    void set_observer(DocumentObserver* observer) { observer_ = observer; }

    // Adds an untitled document in a new tab and makes it active.
    void add_untitled();

    // Opens the file at path (absolute) in a new tab, titled title, and loads it with the scanned
    // format; the tab cannot be edited until the load ends. A binary file opens read-only. If the
    // path is already open, its tab is selected instead.
    void add_file(const std::string& path, const std::string& title, const ScanResult& scan);

    // A document rebuilt from a recovery copy (M2.2c STEP 5).
    struct Recovered {
        // The local path of the file the copy belongs to; nothing for an untitled copy.
        std::optional<std::string> path;
        TextFormat format;
        bool mixed_line_endings = false;
        // The file's time the copy's text was based on (the header's file-mtime), when recorded.
        std::optional<std::int64_t> file_mtime_us;
        // UTF-8 without NUL, at most kRecoveryMaxFileBytes; the caller has checked it.
        std::string text;
    };

    // Adds the document in a new tab, makes it active and returns its id. It is modified, has no
    // undo history and no recovery copy of its own yet (the controller adopts the old one); the
    // observer is told nothing. With a path that is not open it is that file's document, with the
    // copy's format; its first save to that file checks the file's time against file_mtime_us.
    // With no path, or a path that another tab has open, it is an untitled document (the next
    // free "Untitled N"). The insert of the text is timed in the log.
    DocumentId add_recovered(Recovered recovered);

    // Reloads the document from its file with exactly this encoding (Encoding > Reopen as). The
    // text is replaced; a failed load closes the tab like a failed open.
    void reopen(DocumentId id, TextEncoding encoding);

    // Closes the tab without asking; an unknown id is ignored.
    void close(DocumentId id);

    // Shows the record's current title, path and modified flag on its tab (after Save As).
    void refresh_tab(DocumentId id);

    // Keyboard order: wraps around at either end.
    void select_next();
    void select_previous();

    // Selects the tab of id, if it exists.
    void select(DocumentId id);

    std::optional<CursorStatus> active_status() const;

    // The active document's editor page, or nullptr in the blank window (borrowed).
    EditorPage* active_editor() const;

    // The editor page of id, or nullptr (borrowed).
    EditorPage* editor(DocumentId id) const;

private:
    struct Page;

    // A press on a background tab's close button: the notebook selects that tab on the press,
    // before the button sees the click, so the document active before the press is kept here.
    struct ClosePress {
        DocumentId closing = 0;
        DocumentId active_before = 0;
    };

    GtkNotebook* notebook() const;
    Page* find_page(DocumentId id) const;
    Page* find_page_by_widget(GtkWidget* widget) const;
    Page& create_page(DocumentId id);
    void on_modified(DocumentId id, bool modified);
    void on_edited(DocumentId id);
    void on_load_progress(DocumentId id, int percent);
    void on_loaded(DocumentId id, DocumentLoader::Outcome outcome, const std::string& error);
    void on_save_progress(DocumentId id, int percent);
    void on_saved(DocumentId id, DocumentSaver::Outcome outcome, const std::string& error);
    void sync_file_time(DocumentId id);
    void on_press(double x, double y);
    void refresh_label(Page& page);
    void notify_if_active(DocumentId id);
    void on_page_changed();
    void select_active();
    void select_relative(int step, bool wrap);
    void request_close(DocumentId id);
    static void run_pending_closes(gpointer data);

    DocumentList& list_;
    Callbacks callbacks_;
    DocumentObserver* observer_ = nullptr;
    gtkx::ObjectPtr<GtkWidget> box_; // holds the notebook; sees presses before the notebook
    gtkx::ObjectPtr<GtkWidget> notebook_;
    std::vector<std::unique_ptr<Page>> pages_;
    std::vector<DocumentId> pending_closes_;
    guint idle_id_ = 0;
    bool removing_ = false; // true while this object removes a page itself
    std::optional<ClosePress> close_press_;
    gtkx::SignalConnection page_changed_;
    gtkx::SignalConnection reordered_;
    gtkx::SignalConnection pressed_;
};

} // namespace vermilion::ui
