// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the search highlights of the Help page view: the matches, the current match, and the
// steps from match to match.

#include <gtk/gtk.h>

#include <cstddef>
#include <optional>

#include "core/manual_search.h"
#include "core/search_options.h"
#include "ui/help_page_view.h"

namespace vermilion::ui {
namespace {

// Puts `tag` on the characters from `begin` up to but not including `end` of `buffer`.
void tag_range(GtkTextBuffer* buffer, GtkTextTag* tag, std::size_t begin, std::size_t end) {
    GtkTextIter from;
    GtkTextIter to;
    gtk_text_buffer_get_iter_at_offset(buffer, &from, static_cast<int>(begin));
    gtk_text_buffer_get_iter_at_offset(buffer, &to, static_cast<int>(end));
    gtk_text_buffer_apply_tag(buffer, tag, &from, &to);
}

} // namespace

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
        tag_range(buffer_, match_tag_, match.begin, match.end);
    }
}

void HelpPageView::clear_highlights() {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, match_tag_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, current_tag_, &start, &end);
    matches_.clear();
    current_match_.reset();
}

void HelpPageView::make_current(std::size_t index) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, current_tag_, &start, &end);
    current_match_ = index;
    tag_range(buffer_, current_tag_, matches_[index].begin, matches_[index].end);
}

std::optional<MatchStep> HelpPageView::step(bool forward) {
    const std::optional<MatchStep> next = step_match(matches_.size(), current_match_, forward);
    if (next.has_value()) {
        make_current(next->index);
    }
    return next;
}

} // namespace vermilion::ui
