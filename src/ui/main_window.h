// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Main editor window: assembles the menu bar, document tabs and status bar, and owns the actions.

#pragma once

#include <gtk/gtk.h>

#include <string>

#include "core/document_list.h"
#include "ui/document_tabs.h"
#include "ui/file_opener.h"
#include "ui/format_commands.h"
#include "ui/gtkx/signal.h"
#include "ui/save_controller.h"
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

    // Window actions; the action table in window_actions.cpp calls these. Each one works on the
    // active document and does nothing in the blank window.
    void new_file();
    void open_file();
    void save();
    void save_as();
    void close_active();
    void quit();
    void edit(const char* widget_action); // runs e.g. "clipboard.copy" on the active editor
    void next_tab();
    void previous_tab();
    void set_word_wrap(bool wrap);
    void reopen_as(const std::string& encoding_id);
    void save_with_encoding(const std::string& encoding_id);
    void set_line_ending(const std::string& line_ending_id);
    void about();

private:
    bool on_close_request(); // true stops the window from closing
    void refresh();
    void set_enabled(const char* action, bool enabled);
    void show_error(const std::string& message, const std::string& detail);

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window, this object dies with it
    DocumentList list_;
    StatusBar status_;
    DocumentTabs tabs_;      // after list_ and status_, which it uses
    FileOpener opener_;      // after tabs_ and status_, which it uses
    SaveController saves_;   // after tabs_; DocumentTabs reports saves to it
    FormatCommands formats_; // after tabs_
    bool quit_confirmed_ = false;
    gtkx::SignalConnection close_request_;
};

} // namespace vermilion::ui
