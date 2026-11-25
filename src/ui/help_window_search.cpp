// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The search of the Help window: running a search over the manual, stepping through the matches of
// the page on the screen, and the status line that says what happened.

#include <string>
#include <utility>

#include "core/manual_results.h"
#include "core/search_messages.h"
#include "ui/help_window.h"

namespace vermilion::ui {

bool HelpWindow::search_tab_shown() const {
    return gtk_notebook_get_current_page(GTK_NOTEBOOK(tabs_)) == 1;
}

void HelpWindow::show_search() {
    gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs_), 1);
    search_->focus_text();
}

std::string HelpWindow::status_text() const {
    return gtk_label_get_text(GTK_LABEL(status_)); // borrowed
}

void HelpWindow::set_status(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(status_), text.c_str());
}

void HelpWindow::run_search(const SearchOptions& options) {
    const ManualData* manual = library_.data();
    if (manual == nullptr) {
        set_status("The manual is still loading.");
        return;
    }
    page_view_->set_search(options);
    std::vector<PageResult> results;
    if (!search_enabled(options)) {
        set_status("");
    } else if (const std::optional<std::string> error = pattern_error(options)) {
        set_status(pattern_problem_message(*error));
    } else {
        results = search_pages(manual_search_pages(*manual, page_view_->columns()), options);
        set_status(results_message(results));
    }
    search_->set_results(results);
}

void HelpWindow::step_match(bool forward) {
    const std::optional<MatchStep> step = page_view_->step(forward);
    if (!step.has_value()) {
        update_match_status();
        return;
    }
    if (step->wrapped) {
        set_status(wrapped_message(forward));
    } else {
        set_status(match_count_message(static_cast<int>(step->index + 1),
                                       static_cast<int>(page_view_->matches().size())));
    }
}

void HelpWindow::open_result(const std::string& file) {
    open_topic(file);
    if (page_view_->current_match().has_value()) {
        update_match_status();
    } else {
        step_match(true); // the page opens at the match that the result row showed
    }
}

void HelpWindow::update_match_status() {
    const SearchOptions& options = page_view_->search();
    if (!search_enabled(options)) {
        set_status("");
        return;
    }
    if (const std::optional<std::string> error = pattern_error(options)) {
        set_status(pattern_problem_message(*error));
        return;
    }
    const std::optional<std::size_t> current = page_view_->current_match();
    set_status(match_count_message(current.has_value() ? static_cast<int>(*current + 1) : 0,
                                   static_cast<int>(page_view_->matches().size())));
}

} // namespace vermilion::ui
