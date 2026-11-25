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
    highlight_page(std::nullopt);
    return matches_.size();
}

void HelpPageView::highlight_page(std::optional<std::size_t> keep_match_at) {
    const std::size_t old_count = matches_.size(); // of the page that was on the screen
    const std::optional<std::size_t> old_current = current_match_;
    clear_highlights();
    if (search_enabled(search_)) {
        matches_ = find_matches(page_.text, search_);
    }
    for (const SearchMatch& match : matches_) {
        tag_range(buffer_, match_tag_, match.begin, match.end);
    }
    if (!keep_match_at.has_value() || matches_.empty()) {
        return;
    }
    if (old_current.has_value() && old_count == matches_.size()) {
        make_current(*old_current, false);
        return;
    }
    std::size_t index = matches_.size() - 1; // the last match when none ends after the offset
    for (std::size_t i = 0; i < matches_.size(); ++i) {
        if (matches_[i].end > *keep_match_at) {
            index = i;
            break;
        }
    }
    make_current(index, false);
}

void HelpPageView::clear_highlights() {
    if (match_mark_ != nullptr && scroll_mark_ == match_mark_) {
        stop_heading_scroll(); // the scroll would go to the mark that is deleted below
    }
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, match_tag_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, current_tag_, &start, &end);
    if (match_mark_ != nullptr) {
        gtk_text_buffer_delete_mark(buffer_, match_mark_);
        match_mark_ = nullptr;
    }
    matches_.clear();
    current_match_.reset();
}

void HelpPageView::make_current(std::size_t index, bool scroll) {
    if (match_mark_ != nullptr && scroll_mark_ == match_mark_) {
        stop_heading_scroll(); // the scroll would go to the mark that is deleted below
    }
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer_, &start, &end);
    gtk_text_buffer_remove_tag(buffer_, current_tag_, &start, &end);
    if (match_mark_ != nullptr) {
        gtk_text_buffer_delete_mark(buffer_, match_mark_);
        match_mark_ = nullptr;
    }
    current_match_ = index;
    const SearchMatch& match = matches_[index];
    tag_range(buffer_, current_tag_, match.begin, match.end);
    GtkTextIter at;
    gtk_text_buffer_get_iter_at_offset(buffer_, &at, static_cast<int>(match.begin));
    match_mark_ = gtk_text_buffer_create_mark(buffer_, nullptr, &at, TRUE);
    if (scroll) {
        start_scroll(match_mark_, true);
    }
}

std::optional<MatchStep> HelpPageView::step(bool forward) {
    const std::optional<MatchStep> next = step_match(matches_.size(), current_match_, forward);
    if (next.has_value()) {
        make_current(next->index, true);
    }
    return next;
}

} // namespace vermilion::ui
