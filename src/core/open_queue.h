// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The line of files waiting to be opened, one at a time.

#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace vermilion {

// A state machine with no I/O: the caller opens the file that next() returns and calls
// finished() when that file is done (opened, refused, cancelled or failed).
class OpenQueue {
public:
    // Appends the paths in order. Nothing starts.
    void push(const std::vector<std::string>& paths);

    // The next path, which marks the queue busy; nothing while it is busy or empty.
    std::optional<std::string> next();

    // The file that next() returned is done: the queue is no longer busy. Does nothing when it
    // is not busy.
    void finished();

    // Forgets the waiting paths. A file that is running stays busy until finished().
    void clear();

    bool busy() const { return busy_; }

    // How many paths wait (the running one is not counted).
    std::size_t pending() const { return pending_.size(); }

private:
    std::deque<std::string> pending_;
    bool busy_ = false;
};

} // namespace vermilion
