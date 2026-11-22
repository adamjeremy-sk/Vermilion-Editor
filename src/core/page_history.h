// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The pages the help window has shown, for its Back and Forward buttons: the current page and two
// lists.

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

    // Makes page the current page; the previous current page goes onto the back list, and the
    // forward list is emptied. False, and nothing changes (the forward list stays), when page is
    // empty or is already the current page.
    bool open(const std::string& page);

    bool can_go_back() const { return !back_.empty(); }
    bool can_go_forward() const { return !forward_.empty(); }

    // Makes the most recent earlier page the current page and returns it; the page left goes onto
    // the forward list. Nothing, and no change, when the back list is empty.
    std::optional<std::string> back();

    // The reverse of back(): makes the page that back() left most recently the current page and
    // returns it; the page left goes onto the back list. Nothing, and no change, when the forward
    // list is empty.
    std::optional<std::string> forward();

    // The current page; empty before the first open().
    const std::string& current() const { return current_; }

private:
    std::string current_;
    std::vector<std::string> back_;    // oldest first
    std::vector<std::string> forward_; // the page back() left last is at the end
};

} // namespace vermilion
