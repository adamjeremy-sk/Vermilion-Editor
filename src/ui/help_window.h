// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Help → Index window: the manual, one page at a time, with Back and Index buttons.

#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "core/page_history.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

class HelpWindow {
public:
    // Creates the window for app, transient for parent (borrowed; the help window is destroyed
    // with it) and shows the page "index.md". GTK owns the window; this object must be destroyed
    // when the window emits "destroy".
    HelpWindow(GtkApplication* app, GtkWindow* parent);

    HelpWindow(const HelpWindow&) = delete;
    HelpWindow& operator=(const HelpWindow&) = delete;

    GtkWindow* window() const { return window_; }

    void present();

    // The window's actions ("win.back" and "win.to-index"); their buttons and Alt+Left call these.
    void back();
    void show_index();

private:
    // Makes the topic the current page (history) and shows it; nothing when it already is.
    void open_topic(const std::string& name);
    // Builds the widgets of a topic and puts them into the scrolling area; leaves the history.
    void show(const std::string& name);
    // Back is available only when there is a page to go back to.
    void update_actions();
    // A click on a link of a page; always handled, so GTK never launches anything.
    bool follow_link(const char* uri);

    GtkWindow* window_ = nullptr;           // borrowed; GTK owns the window
    GtkScrolledWindow* scroller_ = nullptr; // borrowed; the window owns it
    PageHistory history_;
    // The "activate-link" connections of the labels of the page being shown; replaced with the
    // page.
    std::vector<gtkx::SignalConnection> link_signals_;
};

} // namespace vermilion::ui
