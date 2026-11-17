// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Replace window: the shared search text, the replacement text, the options, the scope.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ui/gtkx/signal.h"
#include "ui/replace_controller.h"
#include "ui/search_controller.h"

namespace vermilion::ui {

// A small non-modal window over the main window, like the Find window (ARCHITECTURE.md 7.10;
// Adam's design talk of 2026-11-13, topic 5). It holds no search state of its own: the search
// text, the three checkboxes and the status line live in the SearchController, the replacement
// text and the scope in the ReplaceController; both belong to the main window and outlive this
// object. Only one of the Find and Replace windows is open at a time, and the controller has one
// status callback: this window sets it in open() and removes it when it hides. Everything runs on
// the main loop.
class ReplaceWindow {
public:
    // What the window needs from the main window.
    struct Host {
        // The text view of the active document (borrowed), or nullptr when no document is open.
        std::function<GtkTextView*()> active_view;
        // The window was closed (Esc, the Close button or the title bar): the main window gives
        // the keyboard focus back to the text. The window is already hidden when this is called.
        std::function<void()> closed;
        // Ctrl+F was pressed in this window: the main window hides it and shows the Find window
        // instead, with the same search text. May be empty.
        std::function<void()> show_find;
        // A Replace All job has just started (the window sets its own state after this returns):
        // the main window locks the document and greys out what must not run meanwhile. It may
        // end the job again at once with cancel_job() when the document cannot be locked.
        std::function<void()> job_started;
        // The job ended, however it ended: the main window unlocks the document. It runs inside
        // the job's done callback, which can be inside a change of tab, so it must not change
        // tabs. May be empty.
        std::function<void()> job_ended;
        // A line for the main status bar: the progress while a job runs and how it ended. May be
        // empty.
        std::function<void(const std::string&)> message;
    };

    // Builds the window, hidden, as a transient child of parent (borrowed; the window is destroyed
    // with it). search and replace (borrowed) must outlive this object. GTK owns the window; this
    // object must be destroyed when the window emits "destroy".
    ReplaceWindow(GtkWindow* parent, SearchController& search, ReplaceController& replace,
                  Host host);

    // Removes the search controller's status callback when this window is the one that set it.
    ~ReplaceWindow();

    ReplaceWindow(const ReplaceWindow&) = delete;
    ReplaceWindow& operator=(const ReplaceWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // True while the window is shown.
    bool is_open() const;

    // Brings the shown window forward and changes nothing in it (Search > Replace while a Replace
    // All job runs: the Cancel button is here).
    void present();

    // Shows the window (or brings it forward) with the cursor in "Find what". First the document's
    // selection is remembered for "Selection only" (ReplaceController::capture_selection). Then,
    // when `selection` has a value (the text the user has selected in the document when it fits
    // on one line), it becomes the search text and is applied at once; without it the entry shows
    // the search controller's text. The entries, the checkboxes and the scope show the state of
    // the controllers, the text in "Find what" is selected, and the status callback is set.
    void open(const std::optional<std::string>& selection);

    // Hides the window and calls Host::closed. The match that is selected in the document stays
    // selected. Does nothing when the window is hidden already.
    void close();

    // Hides the window without calling Host::closed (the main window does this when the Find
    // window opens instead) and removes the status callback. Does nothing when hidden already.
    void hide();

    // Find Next as the button does it: the search controller applies the typed text and searches,
    // then the view scrolls to the match. It works while the window is hidden.
    void find_next();

    // Find Previous as the button does it (Shift+Enter in "Find what" does it too): like
    // find_next(), but the search goes backwards.
    void find_previous();

private:
    // The window, the two entries and the five buttons; then build_options() and build_scope().
    void build(GtkWindow* parent);
    // The three checkboxes.
    void build_options(GtkWidget* grid);
    // The two scope radio buttons and the status line.
    void build_scope(GtkWidget* grid);
    void connect_text_signals();
    void connect_option_signals();
    void connect_button_signals();
    void connect_window_signals();
    void connect_keys();
    void on_find_text_changed();
    void on_replacement_changed();
    void on_scope_toggled();
    // Replace and Replace All as the buttons do them (Enter in "Replace with" is Replace): the
    // controller does the work, then the window scrolls to the match or shows the new scope.
    // Replace All starts a job; while one runs the button is Cancel and ends it.
    void replace_once();
    void replace_all();
    // The replace controller's job callbacks: show the progress; show the end and tell the host.
    void on_job_progress();
    void on_job_done(const ReplaceJobResult& result);
    // Greys out the controls that must not be used while a job runs and turns Replace All into
    // Cancel (its access key stays A), or puts everything back.
    void show_job_running(bool running);
    // Copies the controllers' state (or `text` for "Find what") into the entries, the checkboxes
    // and the scope without telling the controllers.
    void sync_widgets(const std::string& text);
    // Shows the scope the controller reports, and greys out "Selection only" when there is no
    // selection to work on.
    void update_scope();
    // Shows the replace controller's status line, in bold when it is a problem.
    void update_status();
    // Scrolls the active view so that the cursor, which Find leaves on the match, is visible.
    void scroll_to_match();

    SearchController& search_;
    ReplaceController& replace_;
    Host host_;
    GtkWindow* window_ = nullptr;     // borrowed; GTK owns the window
    GtkWidget* find_entry_ = nullptr; // the widgets below are borrowed; the window owns them
    GtkWidget* replace_entry_ = nullptr;
    GtkWidget* match_case_ = nullptr;
    GtkWidget* whole_word_ = nullptr;
    GtkWidget* regex_ = nullptr;
    GtkWidget* whole_document_ = nullptr;
    GtkWidget* selection_only_ = nullptr;
    GtkWidget* status_ = nullptr;
    GtkWidget* find_next_button_ = nullptr;
    GtkWidget* find_previous_button_ = nullptr;
    GtkWidget* replace_button_ = nullptr;
    GtkWidget* replace_all_button_ = nullptr;
    GtkWidget* close_button_ = nullptr;
    // True while sync_widgets() or update_scope() set the widgets: their signals must not reach
    // the controllers.
    bool syncing_ = false;
    // True while this window's callback is the search controller's status callback.
    bool listening_ = false;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
