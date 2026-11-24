// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Help > Index window: the manual, one page at a time, with Back and Index buttons.

#pragma once

#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "core/page_history.h"
#include "ui/gtkx/signal.h"
#include "ui/help_page_view.h"

namespace vermilion::ui {

class HelpWindow {
public:
    // Creates the window for app, transient for parent (borrowed; the help window is destroyed
    // with it) and shows the page "index.md". GTK owns the window; this object must be destroyed
    // when the window emits "destroy". The window opens as wide as the page and kHelpPageLines
    // lines high (below the button bar).
    HelpWindow(GtkApplication* app, GtkWindow* parent);

    HelpWindow(const HelpWindow&) = delete;
    HelpWindow& operator=(const HelpWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // The page on the screen (for tests and for the side pane of a later step).
    HelpPageView& page_view() { return *page_view_; }

    void present();

    // The window's actions ("win.back" and "win.to-index"); their buttons and Alt+Left call these.
    void back();
    void show_index();

    // Makes the topic the current page (history) and shows it; nothing when it already is, or
    // when `name` is not a topic file name (is_topic_name). A topic that is not in the program
    // shows a page that says so.
    void open_topic(const std::string& name);

private:
    // Puts the page of a topic into the view; leaves the history.
    void show(const std::string& name);
    // Back is available only when there is a page to go back to.
    void update_actions();

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window
    PageHistory history_;
    std::unique_ptr<HelpPageView> page_view_;
    // "notify::gtk-application-prefer-dark-theme" on the default GtkSettings: the program's Style
    // changes that flag after it has loaded a new palette, so the page reads its colours again.
    gtkx::SignalConnection palette_signal_;
};

} // namespace vermilion::ui
