// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Search tab of the Help window: the search box with its options, Previous and Next, and one
// result row for every page of the manual that has a match.

#pragma once

#include <gtk/gtk.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "core/manual_results.h"
#include "core/search_options.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// CSS classes of the tab, of its list of results and of the snippet under a result's title.
inline constexpr char kHelpSearchClass[] = "vm-help-search";
inline constexpr char kHelpResultsClass[] = "vm-help-results";
inline constexpr char kHelpSnippetClass[] = "vm-help-snippet";

// What the status line says after a search was run over the manual: "Not found" when no page has
// a match, otherwise "17 matches on 3 pages" ("1 match on 1 page"); both numbers go through
// with_commas().
std::string results_message(const std::vector<PageResult>& results);

// The Search tab. Main loop only. It holds the controls and the list of results and tells its owner
// what the reader did; the owner does the search (core/manual_results.h), shows its message and
// fills the list (set_results()). Nothing is searched until the reader presses Enter in the box:
// changing the text or an option only changes what options() returns.
class HelpSearch {
public:
    // `on_run`: the reader pressed Enter in the box (or run() was called) with these options.
    using RunHandler = std::function<void(const SearchOptions&)>;
    // `on_open`: the reader clicked the row of the page `file`.
    using OpenHandler = std::function<void(const std::string& file)>;
    // `on_step`: the reader pressed Next (true) or Previous (false).
    using StepHandler = std::function<void(bool forward)>;

    // Builds the tab with an empty box, every option off and no results. A handler may be empty.
    HelpSearch(RunHandler on_run, OpenHandler on_open, StepHandler on_step);

    ~HelpSearch();

    HelpSearch(const HelpSearch&) = delete;
    HelpSearch& operator=(const HelpSearch&) = delete;

    // The widget to put into a window; floating until then.
    GtkWidget* widget() const { return root_; }

    // What the box and the three check boxes (Match case, Whole word, Regex) hold now.
    SearchOptions options() const;
    // Puts `options` into the box and the check boxes. Does not run the search.
    void set_options(const SearchOptions& options);

    // Gives the keyboard focus to the box and selects its text.
    void focus_text();

    // What Enter in the box does: calls the run handler with options().
    void run();
    // What the buttons do: calls the step handler.
    void step(bool forward);

    // Replaces the list with one row for every page of `results`, in order.
    void set_results(const std::vector<PageResult>& results);
    // The pages that the list shows, top to bottom.
    const std::vector<PageResult>& results() const { return results_; }
    // What a click on the row at `position` does: calls the open handler with the file of its
    // page. Nothing for a position past the end.
    void activate_row(std::size_t position);

private:
    void setup_row(GObject* item);
    void bind_row(GObject* item);

    RunHandler on_run_;
    OpenHandler on_open_;
    StepHandler on_step_;
    std::vector<PageResult> results_;

    GtkWidget* root_ = nullptr;       // floating until the caller takes it
    GtkWidget* entry_ = nullptr;      // borrowed: the box, owned by root_
    GtkWidget* match_case_ = nullptr; // borrowed
    GtkWidget* whole_word_ = nullptr; // borrowed
    GtkWidget* regex_ = nullptr;      // borrowed
    GtkStringList* model_ = nullptr;  // borrowed: owned by the list view's selection
    gtkx::SignalConnection entry_signal_;
    gtkx::SignalConnection previous_signal_;
    gtkx::SignalConnection next_signal_;
    gtkx::SignalConnection setup_signal_;
    gtkx::SignalConnection bind_signal_;
    gtkx::SignalConnection activate_signal_;
};

} // namespace vermilion::ui
