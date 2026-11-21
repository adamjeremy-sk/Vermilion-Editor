// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The editor font on the display: one CSS provider that holds the rule of
// font_css().

#pragma once

#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// Owns the second CSS provider of the program (the first is Style's): the one
// rule that gives the text view of every editor page, the widgets that carry
// the class vm-editor, their font. The provider is on the default display at
// APPLICATION priority and is added after Style's, so on a tie its rule wins.
// While the font is empty the provider holds no rule and the text keeps the
// system monospace font. Create it once, after the Style, and keep it for the
// life of the program. Main loop only.
class FontStyle {
    // Only install() can make this, so only install() can call the constructor
    // below.
    struct Token {
        explicit Token() = default;
    };

public:
    // Adds the provider, empty, to the default display. Returns null, with the
    // error logged and nothing installed, when there is no display.
    static std::unique_ptr<FontStyle> install();

    // Takes the provider off the display; the text goes back to the system
    // monospace font.
    ~FontStyle();

    FontStyle(const FontStyle&) = delete;
    FontStyle& operator=(const FontStyle&) = delete;

    // Makes `description` (a Pango font description, see font_css() in
    // ui/font_css.h) the editor font: loads font_css(description) into the
    // provider, so widgets that are open follow at once. Empty text, or a
    // description with nothing to write, removes the rule. The same text as now
    // changes nothing. The text is kept as given.
    void set_font(const std::string& description);

    // The text of the last set_font(); empty before the first call.
    const std::string& font() const { return font_; }

    // Not for callers: it needs a Token, which only install() can make.
    FontStyle(Token, gtkx::ObjectPtr<GtkCssProvider> provider,
              gtkx::SignalConnection parsing_errors);

private:
    gtkx::ObjectPtr<GtkCssProvider> provider_; // full reference; the display holds its own
    gtkx::SignalConnection parsing_errors_;    // logs CSS errors; ends with the provider
    std::string font_;
};

} // namespace vermilion::ui
