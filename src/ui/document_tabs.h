// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Document tabs: a notebook with one editor page per DocumentList record, kept in the same order.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "core/document_list.h"
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
    };

    // list must outlive this object.
    DocumentTabs(DocumentList& list, Callbacks callbacks);
    ~DocumentTabs();

    DocumentTabs(const DocumentTabs&) = delete;
    DocumentTabs& operator=(const DocumentTabs&) = delete;

    GtkWidget* widget() const { return box_.get(); }

    // Adds an untitled document in a new tab and makes it active.
    void add_untitled();

    // Closes the tab without asking; an unknown id is ignored.
    void close(DocumentId id);

    // Keyboard order: wraps around at either end.
    void select_next();
    void select_previous();

    std::optional<CursorStatus> active_status() const;

    // The active document's editor, or nullptr in the blank window (borrowed).
    GtkWidget* active_view() const;

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
    void attach_signals(Page& page);
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
