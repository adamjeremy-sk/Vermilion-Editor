// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The colour mode (light, dark or follow the system) and the decision between light and dark.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace vermilion {

// The user's choice. System means: whatever the desktop says.
enum class ColourMode { Light, Dark, System };

// The text kept in settings.ini: "light", "dark" or "system".
const char* colour_mode_id(ColourMode mode);

// The inverse of colour_mode_id. Blanks (space, tab) around the text are ignored and the case of
// ASCII letters does not matter. Anything else, the empty text included, gives System.
ColourMode parse_colour_mode(std::string_view text);

// What the desktop's settings portal means by its "color-scheme" value: 1 is "prefer dark" (true),
// 2 is "prefer light" (false), 0 is "no preference" and any other value is unknown (nothing).
std::optional<bool> portal_dark_from_color_scheme(std::uint32_t value);

// What the desktop says, gathered by the interface layer.
struct SystemHints {
    // The answer of the desktop's settings portal: true for dark, false for light, nothing when
    // there is no answer (no portal, or the portal has no preference).
    std::optional<bool> portal_dark;
    // The name of the GTK theme in use, for example "Greybird-dark".
    std::string theme_name;
    // Whether GTK was set to prefer a dark theme before this program set anything.
    bool prefer_dark_at_start = false;
};

// Whether the interface is dark. Light gives false and Dark gives true, whatever the hints say.
// System takes the first answer found: the portal's answer when there is one; otherwise true when
// the theme name ends in "-dark" (the case of ASCII letters does not matter, and the name must be
// longer than "-dark"); otherwise the flag as it was at the start.
bool resolve_dark(ColourMode mode, const SystemHints& hints);

} // namespace vermilion
