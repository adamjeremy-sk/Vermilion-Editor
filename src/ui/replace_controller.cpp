// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Replace window's logic: the replacement text, the scope, Replace and Replace All.

#include "ui/replace_controller.h"

#include <gtksourceview/gtksource.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/search_messages.h"
#include "core/search_replacement.h"

namespace vermilion::ui {

ReplaceController::ReplaceController(SearchController& search,
                                     std::unique_ptr<DebounceTimer> job_timer)
    : search_(search), timer_(std::move(job_timer)) {
    // A tab change ends a running job while its session still exists.
    search_.set_on_buffer_changing([this]() {
        if (job_.has_value()) {
            finish_job(ReplaceJobEnd::BufferChanged, true);
        }
    });
}

ReplaceController::~ReplaceController() {
    search_.set_on_buffer_changing(nullptr);
    if (job_.has_value()) {
        finish_job(ReplaceJobEnd::Cancelled, false);
    }
    drop_range();
}

void ReplaceController::set_replacement(const std::string& text) {
    replacement_ = text;
}

GtkTextBuffer* ReplaceController::text_buffer() const {
    if (!search_.has_buffer()) {
        return nullptr;
    }
    return GTK_TEXT_BUFFER(search_.buffer());
}

std::optional<std::string> ReplaceController::replacement_problem() const {
    if (text_buffer() == nullptr || search_.status_is_problem()) {
        return std::nullopt;
    }
    std::optional<std::string> error = replacement_error(search_.options(), replacement_);
    if (!error.has_value()) {
        return std::nullopt;
    }
    return replacement_problem_message(*error);
}

void ReplaceController::remember_range(const GtkTextIter& start, const GtkTextIter& end) {
    drop_range();
    GtkTextBuffer* buffer = text_buffer();
    if (buffer == nullptr) {
        return;
    }
    GtkTextIter start_iter = start;
    GtkTextIter end_iter = end;
    GtkTextMark* start_mark = gtk_text_buffer_create_mark(buffer, nullptr, &start_iter, TRUE);
    GtkTextMark* end_mark = gtk_text_buffer_create_mark(buffer, nullptr, &end_iter, FALSE);
    range_start_ = gtkx::ObjectPtr<GtkTextMark>::ref(start_mark);
    range_end_ = gtkx::ObjectPtr<GtkTextMark>::ref(end_mark);
}

void ReplaceController::drop_range() {
    if (range_start_) {
        GtkTextBuffer* owner = gtk_text_mark_get_buffer(range_start_.get());
        if (owner != nullptr) {
            gtk_text_buffer_delete_mark(owner, range_start_.get());
        }
    }
    if (range_end_) {
        GtkTextBuffer* owner = gtk_text_mark_get_buffer(range_end_.get());
        if (owner != nullptr) {
            gtk_text_buffer_delete_mark(owner, range_end_.get());
        }
    }
    range_start_.reset();
    range_end_.reset();
}

bool ReplaceController::range_offsets(int* from, int* to) const {
    GtkTextBuffer* buffer = text_buffer();
    if (buffer == nullptr || !range_start_ || !range_end_) {
        return false;
    }
    if (gtk_text_mark_get_buffer(range_start_.get()) != buffer ||
        gtk_text_mark_get_buffer(range_end_.get()) != buffer) {
        return false;
    }
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_mark(buffer, &start, range_start_.get());
    gtk_text_buffer_get_iter_at_mark(buffer, &end, range_end_.get());
    *from = gtk_text_iter_get_offset(&start);
    *to = gtk_text_iter_get_offset(&end);
    return *from < *to;
}

void ReplaceController::capture_selection() {
    drop_range();
    scope_ = ReplaceScope::WholeDocument;
    GtkTextBuffer* buffer = text_buffer();
    if (buffer == nullptr) {
        return;
    }
    GtkTextIter start;
    GtkTextIter end;
    if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        return;
    }
    remember_range(start, end);
    if (gtk_text_iter_get_line(&start) != gtk_text_iter_get_line(&end)) {
        scope_ = ReplaceScope::SelectionOnly;
    }
}

bool ReplaceController::selection_available() const {
    int from;
    int to;
    if (range_offsets(&from, &to)) {
        return true;
    }
    GtkTextBuffer* buffer = text_buffer();
    if (buffer == nullptr) {
        return false;
    }
    GtkTextIter start;
    GtkTextIter end;
    return gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
}

bool ReplaceController::choose_scope(ReplaceScope scope) {
    if (scope == ReplaceScope::WholeDocument) {
        scope_ = ReplaceScope::WholeDocument;
        return true;
    }
    GtkTextBuffer* buffer = text_buffer();
    if (buffer == nullptr) {
        return false;
    }
    GtkTextIter start;
    GtkTextIter end;
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        remember_range(start, end);
        scope_ = ReplaceScope::SelectionOnly;
        return true;
    }
    int from;
    int to;
    if (range_offsets(&from, &to)) {
        scope_ = ReplaceScope::SelectionOnly;
        return true;
    }
    return false;
}

ReplaceScope ReplaceController::scope() const {
    if (scope_ == ReplaceScope::SelectionOnly) {
        int from;
        int to;
        if (!range_offsets(&from, &to)) {
            return ReplaceScope::WholeDocument;
        }
    }
    return scope_;
}

ReplaceResult ReplaceController::replace() {
    if (job_.has_value()) {
        return ReplaceResult();
    }
    search_.apply();
    SearchSession* session = search_.session();
    if (session == nullptr) {
        return ReplaceResult();
    }
    ReplaceResult result = session->replace(replacement_);
    if (result.refused) {
        return result;
    }
    if (result.replaced && !result.next.found) {
        search_.show_message(replaced_message(1));
    } else if (result.next.found && result.next.wrapped) {
        search_.show_message(wrapped_message(true));
    } else {
        search_.show_message("");
    }
    return result;
}

std::string ReplaceController::status() const {
    if (job_.has_value()) {
        return replacing_message(job_->replaced, job_->total);
    }
    std::optional<std::string> problem = replacement_problem();
    if (problem.has_value()) {
        return *problem;
    }
    return search_.status();
}

bool ReplaceController::status_is_problem() const {
    return !job_.has_value() && (search_.status_is_problem() || replacement_problem().has_value());
}

} // namespace vermilion::ui
