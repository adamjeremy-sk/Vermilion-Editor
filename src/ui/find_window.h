// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Find window: the shared search text, three checkboxes, a status line and the find buttons.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ui/gtkx/signal.h"
#include "ui/search_controller.h"

namespace vermilion::ui {

// A small non-modal window over the main window (ARCHITECTURE.md 7.10; Adam's design talk of
// 2026-11-13). It holds no search state of its own: the text, the three checkboxes and the status
// line live in the SearchController that the main window owns and that the Replace window will
// share, so closing and reopening the window keeps the search. Everything runs on the main loop.
class FindWindow {
public:
    // What the window needs from the main window.
    struct Host {
        // The text view of the active document (borrowed), or nullptr when no document is open.
        std::function<GtkTextView*()> active_view;
        // The window was closed (Esc, the Close button or the title bar): the main window gives
        // the keyboard focus back to the text. The window is already hidden when this is called.
        std::function<void()> closed;
        // Ctrl+H was pressed in this window: the main window hides it and shows the Replace window
        // instead, with the same search text. May be empty.
        std::function<void()> show_replace;
    };

    // Builds the window, hidden, as a transient child of parent (borrowed; the window is destroyed
    // with it). controller (borrowed) must outlive this object. GTK owns the window; this object
    // must be destroyed when the window emits "destroy".
    FindWindow(GtkWindow* parent, SearchController& controller, Host host);

    // Removes the controller's status callback when this window is the one that set it.
    ~FindWindow();

    FindWindow(const FindWindow&) = delete;
    FindWindow& operator=(const FindWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // True while the window is shown.
    bool is_open() const;

    // Shows the window (or brings it forward) with the cursor in "Find what". `selection` is the
    // text the user has selected in the document when it fits on one line, or nothing: it becomes
    // the search text and is applied at once. Without it the entry shows the controller's text.
    // Either way the text in the entry is selected, and the entry and the checkboxes show the
    // controller's state.
    void open(const std::optional<std::string>& selection);

    // Hides the window and calls Host::closed. The match that is selected in the document stays
    // selected. Does nothing when the window is hidden already.
    void close();

    // Hides the window without calling Host::closed (the main window does this when the Replace
    // window opens instead) and removes the status callback. Does nothing when hidden already.
    void hide();

    // Find Next and Find Previous as the buttons do them: the controller applies the typed text
    // and searches, then the view scrolls to the match. They work while the window is hidden.
    void find_next();
    void find_previous();

private:
    void build(GtkWindow* parent);
    void build_options(GtkWidget* grid);
    void connect_signals();
    void connect_keys();
    void on_text_changed();
    // Copies the controller's text (or `text` when given) and options into the entry and the
    // checkboxes without telling the controller.
    void sync_widgets(const std::string& text);
    // Shows the controller's status line, in bold when it is the problem text of a pattern.
    void update_status();
    // Scrolls the active view so that the cursor, which Find leaves on the match, is visible.
    void scroll_to_match();

    SearchController& controller_;
    Host host_;
    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window
    GtkWidget* entry_ = nullptr;  // the widgets below are borrowed; the window owns them
    GtkWidget* match_case_ = nullptr;
    GtkWidget* whole_word_ = nullptr;
    GtkWidget* regex_ = nullptr;
    GtkWidget* status_ = nullptr;
    GtkWidget* find_next_ = nullptr;
    GtkWidget* find_previous_ = nullptr;
    GtkWidget* close_ = nullptr;
    // True while sync_widgets() sets the widgets: their signals must not reach the controller.
    bool syncing_ = false;
    // True while this window's callback is the controller's status callback (set in open()).
    bool listening_ = false;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
