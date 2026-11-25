// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Main editor window: assembles the menu bar, document tabs and status bar, and owns the actions.

#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/settings_file.h"
#include "ui/appearance_controller.h"
#include "ui/appearance_host.h"
#include "ui/document_tabs.h"
#include "ui/external_change_controller.h"
#include "ui/file_opener.h"
#include "ui/find_window.h"
#include "ui/format_commands.h"
#include "ui/goto_line_window.h"
#include "ui/gtkx/signal.h"
#include "ui/help_window.h"
#include "ui/manual_library.h"
#include "ui/recovery_controller.h"
#include "ui/recovery_prompt.h"
#include "ui/replace_controller.h"
#include "ui/replace_window.h"
#include "ui/save_controller.h"
#include "ui/search_controller.h"
#include "ui/settings_window.h"
#include "ui/status_bar.h"
#include "ui/system_theme.h"
#include "ui/tabs_external_host.h"

namespace vermilion::ui {

class MainWindow {
public:
    // Loads the menu bar from the resources and sets the shortcuts of ARCHITECTURE.md 7.10 on
    // app. Call once, before the first window is created.
    static void install_menus_and_shortcuts(GtkApplication* app);

    // Creates the window with one untitled document. GTK owns the window; this object must be
    // destroyed when the window emits "destroy". `style` and `font_style` are borrowed, may be null
    // (then that part of the look is not changed) and must outlive the window: the settings give
    // them the palette and the editor font (start_appearance, M2.5). `manual` is borrowed and must
    // outlive the window: the help window reads its pages there.
    MainWindow(GtkApplication* app, Style* style, FontStyle* font_style, ManualLibrary& manual);

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
    void reload_from_disk(); // File > Reload from Disk (Ctrl+R); may ask first (M2.4)
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
    void show_help();  // Help > Index: opens the help window, or brings it forward
    void find();       // Search > Find: shows the Find window for the active document
    void replace();    // Search > Replace: shows the Replace window for the active document
    void go_to_line(); // Search > Go to Line: shows the Go to Line window for the active document
    // Search > Find Next (F3) and Find Previous (Shift+F3): the next or previous match of the
    // shared search, with the Find and Replace windows open or closed. Before anything has been
    // searched they open the Find window instead.
    void find_next();
    void find_previous();

    // Tools > Settings (Ctrl+Comma): shows the Settings window, which is made on first use and kept
    // (hidden) after it closes. What Apply does is in main_window_settings.cpp.
    void open_settings();

    // Starts the start-up question about the recovery copies an earlier run left. Call once, after
    // present(); a second call does nothing. The code is in main_window_recovery.cpp.
    void offer_recovery();

    // Opens the files named on the command line and shows what was refused (the messages, joined
    // with "; ") on the status bar once the files are done. The files are opened one after the
    // other; the first that opens replaces the blank document of the start while it is still
    // untouched. The code is in main_window_command_line.cpp.
    void open_command_line(const std::vector<std::string>& paths,
                           const std::vector<std::string>& messages);

private:
    bool on_close_request(); // true stops the window from closing
    // The search follows the active document, then the actions and the status bar are brought up
    // to date.
    void refresh();
    // The second half of refresh(): the state of the actions and the status bar. It does not touch
    // the search, so a Replace All job's end can call it from inside a change of buffer.
    void update_state();
    // Closes a document after ending a Replace All job, which has locked it (the close may save).
    void close_document(DocumentId id);
    // Replace All jobs (main_window_search.cpp): the job just started, so the document is locked;
    // the job ended, so it is unlocked; and ending a running job before something acts on its
    // document.
    void replace_job_started();
    void replace_job_ended();
    void end_replace_job();
    void set_enabled(const char* action, bool enabled);
    void show_error(const std::string& message, const std::string& detail);
    // The window's parts of the recovery prompt's Host (main_window_recovery.cpp).
    void close_blank_document();
    // The FileOpener's callbacks for the command line: a file is about to open; the queue is empty.
    // The code is in main_window_command_line.cpp.
    void before_file_opens();
    void on_open_queue_idle();
    RecoveryPrompt::Added add_recovered_document(RecoveryPrompt::Document document);
    // Makes the host and the controller of the look and applies the settings once, before the first
    // document is made, so the first page already has its look. The code is in
    // main_window_appearance.cpp.
    void start_appearance(Style* style, FontStyle* font_style);
    // The search follows the active document (main_window_search.cpp).
    void sync_search_buffer();
    // The selected text when it is on one line and short enough to be a search text.
    std::optional<std::string> selected_search_text() const;
    // Brings the main window forward and puts the keyboard focus in the active text.
    void focus_active_view();
    // The text view of the active document (borrowed), or nullptr without one.
    GtkTextView* active_text_view() const;
    // Shows the Find or the Replace window and hides the other one (without a focus change).
    // `selection` is the text for "Find what"; nullopt keeps the shared search text, which is what
    // the windows' own Ctrl+F and Ctrl+H use.
    void open_find(const std::optional<std::string>& selection);
    void open_replace(const std::optional<std::string>& selection);
    // The work of find_next() and find_previous() (main_window_search.cpp).
    void find_again(bool forward);

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window, this object dies with it
    ManualLibrary& manual_;       // borrowed; the application owns it for the whole session
    DocumentList list_;
    StatusBar status_;
    // The one search of the Find window (and, later, the Replace window). Before tabs_: the tabs
    // may call refresh(), which uses it, while they are destroyed, so it must outlive them.
    SearchController search_;
    // The replacement text and the scope of the Replace window. After search_, which it uses; it
    // is destroyed before search_, whose session keeps the buffer alive for its marks.
    ReplaceController replace_;
    DocumentTabs tabs_;      // after list_ and status_, which it uses
    FileOpener opener_;      // after tabs_ and status_, which it uses
    SaveController saves_;   // after tabs_; DocumentTabs reports saves to it
    FormatCommands formats_; // after tabs_
    // After tabs_ and status_, which its callbacks use, so it is destroyed before them; its
    // destructor deletes the recovery copies of the documents that are still open.
    RecoveryController recovery_;
    // The blank document the window made at start; none once it was closed by the recovery.
    std::optional<DocumentId> blank_id_;
    // Set by open_command_line(), cleared by the first file that opens or when the queue is empty.
    bool close_blank_on_open_ = false;
    // What the command line refused, shown when the queue is empty.
    std::string pending_messages_;
    // The document that a running Replace All job has locked; none without a job.
    std::optional<DocumentId> job_document_;
    // After recovery_ and tabs_, which its Host uses, so it is destroyed before them: a question
    // or a read that is still open then ends without effect. Created by offer_recovery().
    std::unique_ptr<RecoveryPrompt> prompt_;
    bool quit_confirmed_ = false;
    gtkx::SignalConnection close_request_;
    // The user's settings (settings.ini), loaded in the constructor before the host is created.
    // Declared before the host, which borrows it, so it is destroyed after the host.
    SettingsFile settings_{SettingsFile::default_path()};
    // The look at the files of the open documents (M2.4). Created in the constructor after the
    // tabs, which the host uses; the destructor resets them first (the tabs may still call
    // refresh() while they are destroyed), and refresh() does nothing once they are gone.
    std::unique_ptr<TabsExternalHost> external_host_;
    std::unique_ptr<ExternalChangeController> external_;
    // The look that follows the settings: the host gives the palette, the editor look and the
    // limit of the long line to the parts that show them; the controller waits 100 ms after a
    // change. After the tabs, the opener and external_, which the host reaches, so they are
    // destroyed first; the destructor resets them before everything else.
    std::unique_ptr<AppearanceHost> appearance_host_;
    std::unique_ptr<AppearanceController> appearance_;
    // What the desktop says about light and dark; it calls appearance_, so it is destroyed first.
    std::unique_ptr<SystemTheme> system_theme_;
    std::unique_ptr<HelpWindow> help_; // created on first use, reset when its window is destroyed
    gtkx::SignalConnection help_destroy_;
    // After search_ and tabs_, which it uses, so it is destroyed first. Created by find() on first
    // use and reset when its window is destroyed.
    std::unique_ptr<FindWindow> find_;
    gtkx::SignalConnection find_destroy_;
    // Like find_: created by replace() on first use, reset when its window is destroyed.
    std::unique_ptr<ReplaceWindow> replace_window_;
    gtkx::SignalConnection replace_destroy_;
    // Created by go_to_line() on first use, reset when its window is destroyed. Declared last, so
    // it is destroyed first. It uses only the callbacks of its Host, which reach tabs_ and
    // status_.
    std::unique_ptr<GotoLineWindow> goto_;
    gtkx::SignalConnection goto_destroy_;
    // Made by open_settings() on first use and reset when its window is destroyed. Its functions
    // reach settings_, appearance_ and tabs_, which are declared before it.
    std::unique_ptr<SettingsWindow> settings_window_;
    gtkx::SignalConnection settings_window_destroy_;
};

} // namespace vermilion::ui
