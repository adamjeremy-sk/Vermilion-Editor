// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The one search that the Find and Replace windows share: typed text, options, status, Find Next.

#include "ui/search_controller.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/search_messages.h"

namespace vermilion::ui {

SearchController::SearchController(std::unique_ptr<DebounceTimer> timer)
    : timer_(std::move(timer)) {}

void SearchController::set_buffer(GtkSourceBuffer* buffer) {
    if (buffer == buffer_) {
        return;
    }
    if (on_buffer_changing_) {
        std::function<void()> callback = on_buffer_changing_;
        callback();
    }
    session_.reset();
    buffer_ = buffer;
    message_.clear();
    if (buffer != nullptr) {
        session_ = std::make_unique<SearchSession>(buffer);
        session_->set_on_count_changed([this]() { refresh_status(); });
        session_->set_options(applied_);
    }
    refresh_status();
}

void SearchController::set_text(const std::string& text) {
    typed_.text = text;
    if (typed_.text == applied_.text) {
        timer_->stop();
        pending_ = false;
        return;
    }
    pending_ = true;
    timer_->start(kDebounceMs, [this]() { apply(); });
}

void SearchController::apply() {
    timer_->stop();
    pending_ = false;
    apply_options();
}

void SearchController::apply_options() {
    if (applied_ == typed_) {
        return;
    }
    applied_ = typed_;
    message_.clear();
    if (session_ != nullptr) {
        session_->set_options(applied_);
    }
    refresh_status();
}

void SearchController::set_match_case(bool match_case) {
    if (typed_.match_case == match_case) {
        return;
    }
    typed_.match_case = match_case;
    apply();
}

void SearchController::set_whole_word(bool whole_word) {
    if (typed_.whole_word == whole_word) {
        return;
    }
    typed_.whole_word = whole_word;
    apply();
}

void SearchController::set_regex(bool regex) {
    if (typed_.regex == regex) {
        return;
    }
    typed_.regex = regex;
    apply();
}

FindResult SearchController::find_next() {
    return find(true);
}

FindResult SearchController::find_previous() {
    return find(false);
}

FindResult SearchController::find(bool forward) {
    apply();
    message_.clear();
    FindResult result;
    if (session_ != nullptr) {
        result = forward ? session_->find_next() : session_->find_previous();
    }
    if (result.found && result.wrapped) {
        message_ = vermilion::wrapped_message(forward);
    }
    refresh_status();
    return result;
}

void SearchController::show_message(const std::string& message) {
    message_ = message;
    refresh_status();
}

void SearchController::refresh_status() {
    if (frozen_) {
        return;
    }
    std::string text;
    bool problem = false;
    if (!message_.empty()) {
        text = message_;
    } else if (session_ != nullptr && vermilion::search_enabled(applied_)) {
        if (session_->error().has_value()) {
            text = vermilion::pattern_problem_message(*session_->error());
            problem = true;
        } else {
            text = vermilion::match_count_message(session_->position(), session_->count());
        }
    }
    if (text == status_ && problem == problem_) {
        return;
    }
    status_ = std::move(text);
    problem_ = problem;
    if (on_status_changed_) {
        on_status_changed_();
    }
}

void SearchController::set_on_status_changed(std::function<void()> callback) {
    on_status_changed_ = std::move(callback);
}

void SearchController::set_on_buffer_changing(std::function<void()> callback) {
    on_buffer_changing_ = std::move(callback);
}

void SearchController::set_status_frozen(bool frozen) {
    if (frozen_ == frozen) {
        return;
    }
    frozen_ = frozen;
    if (!frozen_) {
        refresh_status();
    }
}

} // namespace vermilion::ui
