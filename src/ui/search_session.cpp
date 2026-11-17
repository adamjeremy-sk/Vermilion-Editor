// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A search over one text buffer: options, count and position, Find Next and Previous, Replace.

#include "ui/search_session.h"

#include <memory>
#include <string>
#include <utility>

#include "core/search_replacement.h"

namespace vermilion::ui {

struct SearchSession::Run {
    std::string replacement;
    int from = 0;
    std::uint64_t count = 0;
    gtkx::ObjectPtr<GtkTextMark> position;
    gtkx::ObjectPtr<GtkTextMark> end;
};

SearchSession::SearchSession(GtkSourceBuffer* buffer)
    : buffer_(gtkx::ObjectPtr<GtkSourceBuffer>::ref(buffer)),
      settings_(gtkx::ObjectPtr<GtkSourceSearchSettings>::adopt(gtk_source_search_settings_new())),
      context_(gtkx::ObjectPtr<GtkSourceSearchContext>::adopt(
          gtk_source_search_context_new(buffer_.get(), settings_.get()))) {
    gtk_source_search_settings_set_wrap_around(settings_.get(), TRUE);
    count_connection_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        context_.get(), "notify::occurrences-count",
        [this](GObject*, GParamSpec*) { count_changed(); });
}

SearchSession::~SearchSession() {
    end_replace_run();
}

void SearchSession::set_options(const SearchOptions& options) {
    options_ = options;
    error_ = pattern_error(options);
    gtk_source_search_settings_set_case_sensitive(settings_.get(), options.match_case);
    gtk_source_search_settings_set_at_word_boundaries(settings_.get(), options.whole_word);
    gtk_source_search_settings_set_regex_enabled(settings_.get(), options.regex);
    const std::string text = active() ? effective_search_text(options) : std::string();
    gtk_source_search_settings_set_search_text(settings_.get(), text.c_str());
}

bool SearchSession::searching() const {
    return active() && gtk_source_search_context_get_occurrences_count(context_.get()) < 0;
}

int SearchSession::count() const {
    if (!active()) {
        return 0;
    }
    return gtk_source_search_context_get_occurrences_count(context_.get());
}

int SearchSession::position() const {
    if (!active()) {
        return 0;
    }
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_selection_bounds(GTK_TEXT_BUFFER(buffer_.get()), &start, &end);
    return gtk_source_search_context_get_occurrence_position(context_.get(), &start, &end);
}

FindResult SearchSession::find_next() {
    return find(true);
}

FindResult SearchSession::find_previous() {
    return find(false);
}

FindResult SearchSession::find(bool forward) {
    FindResult result;
    if (!active()) {
        return result;
    }
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_selection_bounds(GTK_TEXT_BUFFER(buffer_.get()), &start, &end);
    GtkTextIter match_start;
    GtkTextIter match_end;
    gboolean has_wrapped = FALSE;
    const gboolean found =
        forward ? gtk_source_search_context_forward(context_.get(), &end, &match_start, &match_end,
                                                    &has_wrapped)
                : gtk_source_search_context_backward(context_.get(), &start, &match_start,
                                                     &match_end, &has_wrapped);
    if (found == FALSE) {
        return result;
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_.get());
    if (forward) {
        gtk_text_buffer_select_range(buffer, &match_end, &match_start);
    } else {
        gtk_text_buffer_select_range(buffer, &match_start, &match_end);
    }
    result.found = true;
    result.wrapped = (has_wrapped != FALSE);
    return result;
}

ReplaceResult SearchSession::replace(const std::string& replacement) {
    ReplaceResult result;
    if (refuses(replacement)) {
        result.refused = true;
        return result;
    }
    if (!active()) {
        return result;
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_.get());
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    GError* raw = nullptr;
    const gboolean ok = gtk_source_search_context_replace(context_.get(), &start, &end,
                                                          replacement.c_str(), -1, &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (ok != FALSE) {
        result.replaced = true;
        gtk_text_buffer_place_cursor(buffer, &end);
    }
    result.next = find(true);
    return result;
}

namespace {

// What replace_loop did: the matches it replaced, and whether it ended because nothing was left
// (a limit stop leaves finished false).
struct ReplaceLoopResult {
    std::uint64_t count = 0;
    bool finished = false;
};

// The loop of a Replace All run: forward from `position`, replacing the matches that lie wholly
// before `end_mark`, until none is left, one is outside the range, or the library stops. A slice
// may also stop, with finished false, right after a match was replaced and counted: when
// max_matches (0 means no limit) has been reached in this call, or when deadline_us (0 means no
// deadline) has passed at one of the checks, which happen after every 16th match of this call.
ReplaceLoopResult replace_loop(GtkSourceSearchContext* context, GtkTextBuffer* buffer,
                               const std::string& replacement, GtkTextIter* position,
                               GtkTextMark* end_mark, std::uint64_t max_matches,
                               std::int64_t deadline_us) {
    ReplaceLoopResult result;
    for (;;) {
        GtkTextIter match_start;
        GtkTextIter match_end;
        gboolean wrapped = FALSE;
        const gboolean found = gtk_source_search_context_forward(context, position, &match_start,
                                                                 &match_end, &wrapped);
        if (found == FALSE || wrapped != FALSE) {
            result.finished = true;
            break;
        }
        GtkTextIter end_at;
        gtk_text_buffer_get_iter_at_mark(buffer, &end_at, end_mark);
        if (gtk_text_iter_compare(&match_end, &end_at) > 0) {
            result.finished = true;
            break;
        }
        if (gtk_text_iter_equal(&match_start, &match_end)) {
            *position = match_end;
            if (gtk_text_iter_forward_char(position) == FALSE) {
                result.finished = true;
                break;
            }
            continue;
        }
        GError* raw = nullptr;
        const gboolean ok = gtk_source_search_context_replace(context, &match_start, &match_end,
                                                              replacement.c_str(), -1, &raw);
        std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
        if (ok == FALSE) {
            result.finished = true;
            break;
        }
        ++result.count;
        *position = match_end;
        if (max_matches != 0 && result.count >= max_matches) {
            break;
        }
        if (deadline_us != 0 && result.count % 16 == 0 && g_get_monotonic_time() >= deadline_us) {
            break;
        }
    }
    return result;
}

} // namespace

bool SearchSession::begin_replace_run(const std::string& replacement, int from, int to) {
    if (run_ != nullptr || refuses(replacement) || !active()) {
        return false;
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_.get());
    const int char_count = gtk_text_buffer_get_char_count(buffer);
    if (from < 0) {
        from = 0;
    }
    if (to > char_count) {
        to = char_count;
    }
    if (from >= to) {
        return false;
    }
    GtkTextIter position;
    gtk_text_buffer_get_iter_at_offset(buffer, &position, from);
    GtkTextIter limit;
    gtk_text_buffer_get_iter_at_offset(buffer, &limit, to);
    GtkTextMark* position_mark = gtk_text_buffer_create_mark(buffer, nullptr, &position, TRUE);
    GtkTextMark* end_mark = gtk_text_buffer_create_mark(buffer, nullptr, &limit, FALSE);
    gtk_text_buffer_begin_user_action(buffer);
    run_ = std::make_unique<Run>();
    run_->replacement = replacement;
    run_->from = from;
    run_->position = gtkx::ObjectPtr<GtkTextMark>::ref(position_mark);
    run_->end = gtkx::ObjectPtr<GtkTextMark>::ref(end_mark);
    return true;
}

bool SearchSession::replace_run_active() const {
    return run_ != nullptr;
}

ReplaceSlice SearchSession::replace_run_slice(std::uint64_t max_matches, std::int64_t deadline_us) {
    if (run_ == nullptr) {
        return ReplaceSlice{0, true};
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_.get());
    GtkTextIter position;
    gtk_text_buffer_get_iter_at_mark(buffer, &position, run_->position.get());
    const ReplaceLoopResult loop =
        replace_loop(context_.get(), buffer, run_->replacement, &position, run_->end.get(),
                     max_matches, deadline_us);
    run_->count += loop.count;
    if (loop.count > 0) {
        gtk_text_buffer_move_mark(buffer, run_->position.get(), &position);
    }
    return ReplaceSlice{loop.count, loop.finished};
}

ReplaceAllResult SearchSession::end_replace_run() {
    ReplaceAllResult result;
    if (run_ == nullptr) {
        return result;
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_.get());
    gtk_text_buffer_end_user_action(buffer);
    const std::uint64_t count = run_->count;
    if (count > 0) {
        GtkTextIter range_start;
        gtk_text_buffer_get_iter_at_offset(buffer, &range_start, run_->from);
        GtkTextIter range_end;
        gtk_text_buffer_get_iter_at_mark(buffer, &range_end, run_->end.get());
        gtk_text_buffer_select_range(buffer, &range_end, &range_start);
    }
    if (run_->position) {
        GtkTextBuffer* owner = gtk_text_mark_get_buffer(run_->position.get());
        if (owner != nullptr) {
            gtk_text_buffer_delete_mark(owner, run_->position.get());
        }
    }
    if (run_->end) {
        GtkTextBuffer* owner = gtk_text_mark_get_buffer(run_->end.get());
        if (owner != nullptr) {
            gtk_text_buffer_delete_mark(owner, run_->end.get());
        }
    }
    run_.reset();
    result.count = count;
    return result;
}

bool SearchSession::refuses(const std::string& replacement) const {
    return error_.has_value() || replacement_error(options_, replacement).has_value();
}

void SearchSession::set_on_count_changed(std::function<void()> callback) {
    on_count_changed_ = std::move(callback);
}

void SearchSession::count_changed() {
    if (on_count_changed_) {
        std::function<void()> callback = on_count_changed_;
        callback();
    }
}

} // namespace vermilion::ui
