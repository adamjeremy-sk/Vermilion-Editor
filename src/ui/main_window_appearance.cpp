// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// MainWindow::start_appearance: the look of the program follows the settings.

#include "core/editor_settings.h"
#include "ui/glib_timer.h"
#include "ui/main_window.h"
#include "ui/style.h"

namespace vermilion::ui {

void MainWindow::start_appearance(Style* style, FontStyle* font_style) {
    appearance_host_ =
        std::make_unique<AppearanceHost>(style, font_style, tabs_, [this](std::uint64_t limit) {
            opener_.set_long_line_limit(limit);
            if (external_) {
                external_->set_long_line_limit(limit);
            }
        });
    appearance_ =
        std::make_unique<AppearanceController>(*appearance_host_, std::make_unique<GlibTimer>());
    // The hints are kept before start() and used by it, so Follow system is right from the first
    // window; the portal answers later and the theme name can change at any time.
    system_theme_ =
        SystemTheme::create(style != nullptr && style->prefer_dark_at_start(), true,
                            [this](const SystemHints& hints) { appearance_->set_hints(hints); });
    if (system_theme_) {
        appearance_->set_hints(system_theme_->hints());
    }
    appearance_->start(load_settings(settings_));
}

} // namespace vermilion::ui
