// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Main editor window: assembles the menu bar, document tabs and status bar, and owns the actions.

#pragma once

#include <gtk/gtk.h>

#include "core/document_list.h"
#include "ui/document_tabs.h"
#include "ui/gtkx/signal.h"
#include "ui/status_bar.h"

namespace vermilion::ui {

class MainWindow {
public:
    // Loads the menu bar from the resources and sets the shortcuts of ARCHITECTURE.md 7.10 on
    // app. Call once, before the first window is created.
    static void install_menus_and_shortcuts(GtkApplication* app);

    // Creates the window with one untitled document. GTK owns the window; this object must be
    // destroyed when the window emits "destroy".
    explicit MainWindow(GtkApplication* app);

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    GtkWindow* window() const { return window_; }

    void present();

    // Window actions; the action table in main_window.cpp calls these.
    void new_file();
    void close_active();
    void quit();
    void edit(const char* widget_action); // runs e.g. "clipboard.copy" on the active editor
    void next_tab();
    void previous_tab();
    void about();

private:
    void request_close(DocumentId id);
    bool on_close_request(); // true stops the window from closing
    void refresh();
    void set_enabled(const char* action, bool enabled);

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window, this object dies with it
    DocumentList list_;
    StatusBar status_;
    DocumentTabs tabs_; // after list_ and status_, which it uses
    bool quit_confirmed_ = false;
    gtkx::SignalConnection close_request_;
};

} // namespace vermilion::ui
