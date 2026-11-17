// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Go to Line window: one entry for a line number or line:column, the range and a status line.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// A small non-modal window over the main window (ARCHITECTURE.md 7.10; Adam's design talk of
// 2026-11-13). It is independent of the Find and Replace windows: no shared search, no part in
// their one-at-a-time rule. It keeps no state of its own: each Go reads the active document at
// that moment, so a tab change while it is open is followed. All the jumping is jump_to_line()
// (ui/line_jump.h). Everything runs on the main loop.
class GotoLineWindow {
public:
    // What the window needs from the main window.
    struct Host {
        // The text view of the active document (borrowed), or nullptr when no document is open.
        // The view is a GtkSourceView; the window reads its tab width.
        std::function<GtkTextView*()> active_view;
        // The window was closed (a jump, Esc, Cancel or the title bar): the main window gives the
        // keyboard focus back to the text. The window is already hidden when this is called.
        std::function<void()> closed;
        // A message for the main status bar (a line past the end).
        std::function<void(const std::string&)> message;
    };

    // Builds the window, hidden, as a transient child of parent (borrowed; the window is destroyed
    // with it). GTK owns the window; this object must be destroyed when the window emits
    // "destroy".
    GotoLineWindow(GtkWindow* parent, Host host);

    GotoLineWindow(const GotoLineWindow&) = delete;
    GotoLineWindow& operator=(const GotoLineWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // True while the window is shown.
    bool is_open() const;

    // Shows the window (or brings it forward) for the active document: the entry shows the line
    // of the cursor, selected, with the keyboard focus; the range label shows the number of lines;
    // the status line is empty. Does nothing when no document is open.
    void open();

    // Hides the window and calls Host::closed. Does nothing when the window is hidden already.
    void close();

private:
    void build(GtkWindow* parent);
    void connect_signals();
    void connect_keys();
    // The Go button and Enter in the entry: jumps on the active document. A line that exists, or
    // one past the end, closes the window (the second also sends the message to the status bar);
    // text that is not a line stays in the window with the hint in the problem style. Does nothing
    // without a document.
    void go();
    // Sets the range label to "Lines " plus range_text_of() for the active document.
    void refresh_range();
    // Shows `text` in the status line, in bold when `problem` is true; "" empties it.
    void show_status(const std::string& text, bool problem);
    // Scrolls `view` so that the cursor, which the jump left on its line, is in the middle.
    void scroll_to_cursor(GtkTextView* view);

    Host host_;
    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window
    GtkWidget* entry_ = nullptr;  // the widgets below are borrowed; the window owns them
    GtkWidget* range_ = nullptr;
    GtkWidget* status_ = nullptr;
    GtkWidget* go_ = nullptr;
    GtkWidget* cancel_ = nullptr;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
