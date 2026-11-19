// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The line of files waiting to be opened, one at a time.

#include "core/open_queue.h"

#include <utility>

namespace vermilion {

void OpenQueue::push(const std::vector<std::string>& paths) {
    pending_.insert(pending_.end(), paths.begin(), paths.end());
}

std::optional<std::string> OpenQueue::next() {
    if (busy_ || pending_.empty()) {
        return std::nullopt;
    }
    std::string path = std::move(pending_.front());
    pending_.pop_front();
    busy_ = true;
    return path;
}

void OpenQueue::finished() {
    busy_ = false;
}

void OpenQueue::clear() {
    pending_.clear();
}

} // namespace vermilion
