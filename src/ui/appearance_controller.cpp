// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// AppearanceController: decides what changes and gives it to the Host after the 100 ms wait.

#include "ui/appearance_controller.h"

#include <utility>

namespace vermilion::ui {

AppearanceController::AppearanceController(Host& host, std::unique_ptr<DebounceTimer> timer)
    : host_(host), timer_(std::move(timer)) {}

AppearanceController::~AppearanceController() {
    timer_->stop();
}

void AppearanceController::start(const EditorSettings& values) {
    settings_ = normalise_settings(values);
    started_ = true;
    timer_->stop();
    pending_ = false;
    flush();
}

void AppearanceController::apply(const EditorSettings& values) {
    settings_ = normalise_settings(values);
    schedule();
}

void AppearanceController::set_hints(const SystemHints& hints) {
    hints_ = hints;
    schedule();
}

void AppearanceController::schedule() {
    if (!started_) {
        return;
    }
    pending_ = true;
    timer_->start(kDelayMs, [this] {
        pending_ = false;
        flush();
    });
}

void AppearanceController::flush() {
    const bool dark = resolve_dark(settings_.colour_mode, hints_);
    const EditorLook look = make_look(settings_, dark);
    const std::uint64_t limit = settings_.long_line_limit;

    if (!applied_dark_ || *applied_dark_ != dark) {
        applied_dark_ = dark;
        host_.apply_palette(dark);
    }

    if (!applied_look_ || *applied_look_ != look) {
        applied_look_ = look;
        host_.apply_look(look);
    }

    if (!applied_limit_ || *applied_limit_ != limit) {
        applied_limit_ = limit;
        host_.apply_long_line_limit(limit);
    }
}

} // namespace vermilion::ui
