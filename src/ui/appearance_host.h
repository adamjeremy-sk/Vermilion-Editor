// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Host of the appearance controller: gives each part of the look to the part that shows it.

#pragma once

#include <cstdint>
#include <functional>

#include "core/editor_look.h"
#include "ui/appearance_controller.h"
#include "ui/document_tabs.h"
#include "ui/font_style.h"
#include "ui/style.h"

namespace vermilion::ui {

// What the controller decides, done on the program: the palette goes to the Style, the look to the
// tabs (every page, open or later) and its font to the FontStyle, the long-line limit to whoever
// asked for it (a callback, so the host knows neither the opener nor the change controller).
// Main loop only.
class AppearanceHost : public AppearanceController::Host {
public:
    // `style` and `font_style` are borrowed and may be null (a missing stylesheet, no display):
    // that part of the look is then left alone. `tabs` is borrowed. `long_line_limit` may be empty.
    // All of them must outlive the host.
    AppearanceHost(Style* style, FontStyle* font_style, DocumentTabs& tabs,
                   std::function<void(std::uint64_t)> long_line_limit);

    // Style::set_dark(dark) when there is a style.
    void apply_palette(bool dark) override;

    // DocumentTabs::set_look(look), then FontStyle::set_font(look.font) when there is a font style.
    void apply_look(const EditorLook& look) override;

    // Calls the callback with the limit, when there is one.
    void apply_long_line_limit(std::uint64_t limit) override;

private:
    Style* style_;          // borrowed, may be null
    FontStyle* font_style_; // borrowed, may be null
    DocumentTabs& tabs_;    // borrowed
    std::function<void(std::uint64_t)> long_line_limit_;
};

} // namespace vermilion::ui
