// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the search highlights of the Help page view: the matches of the search on the page.

#include <gtk/gtk.h>

#include <cstddef>

#include "core/manual_search.h"
#include "core/search_options.h"
#include "ui/help_page_view.h"

namespace vermilion::ui {

std::size_t HelpPageView::set_search(const SearchOptions& options) {
    search_ = options;
    highlight_page();
    return matches_.size();
}

void HelpPageView::highlight_page() {
    clear_highlights();
    if (search_enabled(search_)) {
        matches_ = find_matches(page_.text, search_);
    }
    for (const SearchMatch& match : matches_) {
        GtkTextIter from;
        GtkTextIter to;
        gtk_text_buffer_get_iter_at_offset(buffer_, &from, static_cast<int>(match.begin));
        gtk_text_buffer_get_iter_at_offset(buffer_, &to, static_cast<int>(match.end));
        gtk_text_buffer_apply_tag(buffer_, match_tag_, &from, &to);
    }
}

void HelpPageView::clear_highlights() {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, match_tag_, &start, &end);
    matches_.clear();
}

} // namespace vermilion::ui
