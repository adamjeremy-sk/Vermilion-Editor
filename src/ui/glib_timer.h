// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The DebounceTimer on the GLib main loop: one g_timeout_add source at a time.

#pragma once

#include <glib.h>

#include <functional>

#include "ui/debounce_timer.h"

namespace vermilion::ui {

class GlibTimer : public DebounceTimer {
public:
    GlibTimer() = default;

    // Removes the source, so a destroyed timer never calls anything.
    ~GlibTimer() override;

    GlibTimer(const GlibTimer&) = delete;
    GlibTimer& operator=(const GlibTimer&) = delete;

    // The call may start the timer again, or destroy it.
    void start(unsigned delay_ms, std::function<void()> fire) override;
    void stop() override;

    // True while a call is waiting.
    bool pending() const { return source_ != 0; }

private:
    static gboolean on_timeout(gpointer data);

    guint source_ = 0; // the id of the GLib source, 0 when none is waiting
    std::function<void()> fire_;
};

} // namespace vermilion::ui
