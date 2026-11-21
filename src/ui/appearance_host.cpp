// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// AppearanceHost: the palette, the look of the pages, the font and the long-line limit.

#include "ui/appearance_host.h"

#include <utility>

namespace vermilion::ui {

AppearanceHost::AppearanceHost(Style* style, FontStyle* font_style, DocumentTabs& tabs,
                               std::function<void(std::uint64_t)> long_line_limit)
    : style_(style), font_style_(font_style), tabs_(tabs),
      long_line_limit_(std::move(long_line_limit)) {}

void AppearanceHost::apply_palette(bool dark) {
    if (style_ != nullptr) {
        style_->set_dark(dark);
    }
}

void AppearanceHost::apply_look(const EditorLook& look) {
    tabs_.set_look(look);
    if (font_style_ != nullptr) {
        font_style_->set_font(look.font);
    }
}

void AppearanceHost::apply_long_line_limit(std::uint64_t limit) {
    if (long_line_limit_) {
        long_line_limit_(limit);
    }
}

} // namespace vermilion::ui
