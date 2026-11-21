// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The program's stylesheet: one CSS provider on the display, with the light or the dark palette.

#pragma once

#include <gtk/gtk.h>

#include <memory>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// Owns the one GtkCssProvider that styles every window: a palette sheet (classic_light.css or
// classic_dark.css, which define the colour names) followed by the structure sheet
// (classic_structure.css, which holds no colour of its own and uses those names). The provider is
// on the default display at APPLICATION priority, over the desktop's own theme; the theme's name is
// never changed. Switching the palette reloads the same provider, so windows that are open follow
// at once. Create it once, after GTK is initialised and the program's resources are registered,
// and keep it for the life of the program. Main loop only.
class Style {
    // Only install() can make this, so only install() can call the constructor below.
    struct Token {
        explicit Token() = default;
    };

public:
    // Builds the provider for the light or the dark palette, adds it to the default display and
    // sets GTK's prefer-dark flag to match `dark` (see set_dark). Returns null, with the error
    // logged and nothing installed, when a stylesheet resource is missing or there is no display.
    static std::unique_ptr<Style> install(bool dark);

    // Takes the provider off the display; the windows go back to the desktop's own look.
    ~Style();

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    // Switches the palette. Does nothing and returns true when `dark` is already the current
    // state. Otherwise reloads the provider with the other palette and sets
    // gtk-application-prefer-dark-theme to `dark`, but only when GTK's value differs: setting an
    // unchanged value makes GTK announce it again, and a listener that reacts to the announcement
    // then loops. Returns false (logged) when a palette resource is missing; the look and the
    // flag stay as they were.
    bool set_dark(bool dark);

    // The palette now in use.
    bool dark() const { return dark_; }

    // The value of gtk-application-prefer-dark-theme as GTK had it before this program first set
    // it (read once, when the Style was installed). Follow-system detection needs it, because
    // once set_dark has run the flag shows our own choice, not the desktop's.
    bool prefer_dark_at_start() const { return prefer_dark_at_start_; }

    // Not for callers: it needs a Token, which only install() can make.
    Style(Token, gtkx::ObjectPtr<GtkCssProvider> provider, gtkx::SignalConnection parsing_errors,
          bool dark, bool prefer_dark_at_start);

private:
    gtkx::ObjectPtr<GtkCssProvider> provider_; // full reference; the display holds its own
    gtkx::SignalConnection parsing_errors_;    // logs CSS errors; ends with the provider
    bool dark_;
    bool prefer_dark_at_start_;
};

} // namespace vermilion::ui
