// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements PageHistory: the current page, a bounded back list and a forward list.

#include "core/page_history.h"

#include <utility>

namespace vermilion {

bool PageHistory::open(const std::string& page) {
    if (page.empty() || page == current_) {
        return false;
    }
    if (!current_.empty()) {
        if (back_.size() == kMaxBack) {
            back_.erase(back_.begin());
        }
        back_.push_back(std::move(current_));
    }
    current_ = page;
    forward_.clear();
    return true;
}

std::optional<std::string> PageHistory::back() {
    if (back_.empty()) {
        return std::nullopt;
    }
    forward_.push_back(std::move(current_));
    current_ = std::move(back_.back());
    back_.pop_back();
    return current_;
}

std::optional<std::string> PageHistory::forward() {
    if (forward_.empty()) {
        return std::nullopt;
    }
    back_.push_back(std::move(current_));
    current_ = std::move(forward_.back());
    forward_.pop_back();
    return current_;
}

} // namespace vermilion
