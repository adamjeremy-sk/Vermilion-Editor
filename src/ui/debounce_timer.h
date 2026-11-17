// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A one-shot timer that a new start() replaces: the pause behind a search typed letter by letter.

#pragma once

#include <functional>

namespace vermilion::ui {

class DebounceTimer {
public:
    virtual ~DebounceTimer() = default;

    // Calls `fire` once, on the main loop, `delay_ms` milliseconds from now. A call that is still
    // waiting is dropped: only the latest start() fires.
    virtual void start(unsigned delay_ms, std::function<void()> fire) = 0;

    // Drops a call that is still waiting; does nothing when none is.
    virtual void stop() = 0;
};

} // namespace vermilion::ui
