// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Main editor window: assembles the menu bar, document tabs and status bar, and owns the actions.

#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "core/document_list.h"
#include "ui/document_tabs.h"
#include "ui/file_opener.h"
#include "ui/format_commands.h"
#include "ui/gtkx/signal.h"
#include "ui/help_window.h"
#include "ui/recovery_controller.h"
#include "ui/recovery_prompt.h"
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

    // Detaches the recovery controller from the document tabs first (see the .cpp).
    ~MainWindow();

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
    void show_help(); // Help > Index: opens the help window, or brings it forward

    // Starts the start-up question about the recovery copies an earlier run left. Call once, after
    // present(); a second call does nothing. The code is in main_window_recovery.cpp.
    void offer_recovery();

private:
    bool on_close_request(); // true stops the window from closing
    void refresh();
    void set_enabled(const char* action, bool enabled);
    void show_error(const std::string& message, const std::string& detail);
    // The window's parts of the recovery prompt's Host (main_window_recovery.cpp).
    void close_blank_document();
    RecoveryPrompt::Added add_recovered_document(RecoveryPrompt::Document document);

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window, this object dies with it
    DocumentList list_;
    StatusBar status_;
    DocumentTabs tabs_;      // after list_ and status_, which it uses
    FileOpener opener_;      // after tabs_ and status_, which it uses
    SaveController saves_;   // after tabs_; DocumentTabs reports saves to it
    FormatCommands formats_; // after tabs_
    // After tabs_ and status_, which its callbacks use, so it is destroyed before them; its
    // destructor deletes the recovery copies of the documents that are still open.
    RecoveryController recovery_;
    // The blank document the window made at start; none once it was closed by the recovery.
    std::optional<DocumentId> blank_id_;
    // After recovery_ and tabs_, which its Host uses, so it is destroyed before them: a question
    // or a read that is still open then ends without effect. Created by offer_recovery().
    std::unique_ptr<RecoveryPrompt> prompt_;
    bool quit_confirmed_ = false;
    gtkx::SignalConnection close_request_;
    std::unique_ptr<HelpWindow> help_; // created on first use, reset when its window is destroyed
    gtkx::SignalConnection help_destroy_;
};

} // namespace vermilion::ui
