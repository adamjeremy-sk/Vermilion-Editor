// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The pages the help window has shown, for its Back button: the current page and a bounded list.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vermilion {

class PageHistory {
public:
    // How many earlier pages are kept; when there are more, the oldest is forgotten.
    static constexpr std::size_t kMaxBack = 100;

    // Makes page the current page; the previous current page goes onto the back list. False, and
    // nothing changes, when page is empty or is already the current page.
    bool open(const std::string& page);

    bool can_go_back() const { return !back_.empty(); }

    // Makes the most recent earlier page the current page and returns it; nothing, and no
    // change, when the back list is empty. The page left is not kept (there is no forward).
    std::optional<std::string> back();

    // The current page; empty before the first open().
    const std::string& current() const { return current_; }

private:
    std::string current_;
    std::vector<std::string> back_; // oldest first
};

} // namespace vermilion
