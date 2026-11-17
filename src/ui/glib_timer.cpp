// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The DebounceTimer on the GLib main loop: one g_timeout_add source at a time.

#include "ui/glib_timer.h"

#include <glib.h>

#include <functional>
#include <utility>

namespace vermilion::ui {

GlibTimer::~GlibTimer() {
    stop();
}

void GlibTimer::start(unsigned delay_ms, std::function<void()> fire) {
    stop();
    fire_ = std::move(fire);
    source_ = g_timeout_add(delay_ms, &GlibTimer::on_timeout, this);
}

void GlibTimer::stop() {
    if (source_ != 0) {
        g_source_remove(source_);
        source_ = 0;
    }
    fire_ = nullptr;
}

gboolean GlibTimer::on_timeout(gpointer data) {
    GlibTimer* self = static_cast<GlibTimer*>(data);
    self->source_ = 0;
    std::function<void()> fire = std::move(self->fire_);
    self->fire_ = nullptr;
    if (fire) {
        fire();
    }
    return G_SOURCE_REMOVE;
}

} // namespace vermilion::ui
