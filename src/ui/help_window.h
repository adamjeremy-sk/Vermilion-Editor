// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Help > Index window: the manual, one page at a time, with the Contents tree and the Search
// tab at its left, Back and Index buttons above and a status line below.

#pragma once

#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "core/page_history.h"
#include "core/search_options.h"
#include "ui/gtkx/signal.h"
#include "ui/help_contents.h"
#include "ui/help_page_view.h"
#include "ui/help_search.h"
#include "ui/manual_library.h"

namespace vermilion::ui {

class HelpWindow {
public:
    // Creates the window for app, transient for parent (borrowed; the help window is destroyed
    // with it) and shows the page "index.md". GTK owns the window; this object must be destroyed
    // when the window emits "destroy". The window opens kHelpOpenColumns columns wide and
    // kHelpPageLines lines high (below the button bar). `library` (borrowed, must outlive this
    // object) holds the pages: while it is not ready the page says "Loading..." and is filled in
    // when the library is ready (the current page of the history, which may have changed).
    HelpWindow(GtkApplication* app, GtkWindow* parent, ManualLibrary& library);

    HelpWindow(const HelpWindow&) = delete;
    HelpWindow& operator=(const HelpWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // The page on the screen (for tests and for the side pane of a later step).
    HelpPageView& page_view() { return *page_view_; }
    // The Contents tree at the left of the page (for tests).
    HelpContents& contents() { return *contents_; }
    // The Search tab (for tests).
    HelpSearch& search() { return *search_; }

    void present();

    // The window's actions ("win.back" and "win.to-index"); their buttons and Alt+Left call these.
    void back();
    void show_index();

    // Makes the topic the current page (history) and shows it; nothing when it already is, or
    // when `name` is not a topic file name (is_topic_name). A topic that is not in the program
    // shows a page that says so.
    void open_topic(const std::string& name);

    // The side pane has two tabs, Contents and Search. True while the Search tab is the one shown.
    bool search_tab_shown() const;
    // Shows the Search tab and puts the keyboard in its box (Ctrl+F, in a later step).
    void show_search();

    // Runs a search over the manual (the Enter key in the Search box): the page on the screen and
    // every page that opens after it show the matches of `options`, the Search tab lists the pages
    // that have a match, and the status line says how many there are or why the search cannot be
    // done. An empty text switches the search off (no highlights, no results). While the manual is
    // not ready nothing is searched and the status line says so.
    void run_search(const SearchOptions& options);
    // Makes the next (forward) or previous match of the page on the screen the current one and
    // says on the status line which one it is ("3 of 17 matches", or that the step wrapped around).
    void step_match(bool forward);
    // The text of the status line at the bottom of the window.
    std::string status_text() const;

private:
    // Puts the page of a topic into the view; leaves the history.
    void show(const std::string& name);
    // Back is available only when there is a page to go back to.
    void update_actions();
    // A click on a result row: opens the page and goes to its first match.
    void open_result(const std::string& file);
    // Sets the status line.
    void set_status(const std::string& text);
    // The status line for the page on the screen and the last search: nothing without a search,
    // the pattern's problem when it is bad, otherwise the number of matches on the page.
    void update_match_status();

    GtkWindow* window_ = nullptr; // borrowed; GTK owns the window
    ManualLibrary& library_;
    // The claim on the library's "ready" call; empty when the library was ready at the start.
    ManualLibrary::Wait wait_;
    PageHistory history_;
    std::unique_ptr<HelpPageView> page_view_;
    std::unique_ptr<HelpContents> contents_;
    std::unique_ptr<HelpSearch> search_;
    GtkWidget* tabs_ = nullptr;   // borrowed: the notebook with the two tabs, owned by the window
    GtkWidget* status_ = nullptr; // borrowed: the label of the status line
    // "notify::gtk-application-prefer-dark-theme" on the default GtkSettings: the program's Style
    // changes that flag after it has loaded a new palette, so the page reads its colours again.
    gtkx::SignalConnection palette_signal_;
};

} // namespace vermilion::ui
