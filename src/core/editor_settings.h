// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The settings of the Settings window as one value, and how they are read from and written to
// settings.ini.

#pragma once

#include <cstdint>
#include <string>

#include "core/colour_mode.h"
#include "core/settings_file.h"
#include "core/settings_keys.h"

namespace vermilion {

struct EditorSettings {
    ColourMode colour_mode = ColourMode::System;
    bool highlight_current_line = settings::kHighlightCurrentLineDefault;
    // A Pango font description; empty text means the system monospace font.
    std::string font = settings::kFontDefault;
    // 4 or 8.
    int tab_width = settings::kTabWidthDefault;
    bool confirm_large_reload = settings::kConfirmLargeReloadDefault;
    // Characters on one line above which opening a file asks first.
    std::uint64_t long_line_limit = settings::kLongLineLimitDefault;
};

inline bool operator==(const EditorSettings& a, const EditorSettings& b) {
    return a.colour_mode == b.colour_mode && a.highlight_current_line == b.highlight_current_line &&
           a.font == b.font && a.tab_width == b.tab_width &&
           a.confirm_large_reload == b.confirm_large_reload &&
           a.long_line_limit == b.long_line_limit;
}

inline bool operator!=(const EditorSettings& a, const EditorSettings& b) {
    return !(a == b);
}

// Every setting at its default.
EditorSettings default_settings();

// Brings every field into its allowed range: a tab width other than 4 or 8 becomes 4; the
// long-line limit is kept between settings::kLongLineLimitMin and settings::kLongLineLimitMax;
// blanks (space, tab) around the font are cut. Nothing else changes. Applying it twice gives the
// same result as applying it once.
EditorSettings normalise_settings(EditorSettings settings);

// Reads every setting from `file` (already load()ed). A missing key, a value of the wrong type or
// a number too large for an int gives the key's default; a colour mode that is not "light",
// "dark" or "system" gives System; a tab width other than 4 or 8 gives 4; a long-line limit
// outside its bounds is moved to the nearest bound. The result is always normalised.
EditorSettings load_settings(const SettingsFile& file);

// Remembers in `file` (set_bool, set_int, set_string; the caller calls save()) the settings that
// differ from what load_settings(file) reads now, so settings.ini holds only keys the user changed
// and a setting changed back to its default is written out again. The values are normalised
// first. Nothing is remembered when nothing differs.
void store_settings(const EditorSettings& settings, SettingsFile& file);

} // namespace vermilion
