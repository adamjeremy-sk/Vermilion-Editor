// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The names of the settings in settings.ini: groups, keys and defaults (a name never changes).

#pragma once

namespace vermilion::settings {

// [appearance]
inline constexpr const char* kAppearanceGroup = "appearance";

// Light, dark or follow the system: the text "light", "dark" or "system".
inline constexpr const char* kColourMode = "colour_mode";
inline constexpr const char* kColourModeDefault = "system";

// The band under the line with the cursor. Boolean.
inline constexpr const char* kHighlightCurrentLine = "highlight_current_line";
inline constexpr bool kHighlightCurrentLineDefault = true;

// [editor]
inline constexpr const char* kEditorGroup = "editor";

// The editor font as a Pango font description, for example "DejaVu Sans Mono 11". Empty text means
// the system monospace font, and that is the default.
inline constexpr const char* kFont = "font";
inline constexpr const char* kFontDefault = "";
// The font the settings page shows (greyed out) while the system monospace font is in use, and
// switches to first when that box is unchecked: the font the program bundles, Fantasque Sans Mono.
inline constexpr const char* kFontFirstChoice = "Fantasque Sans Mono 12";

// How wide a tab is, and how many spaces the Tab key inserts: 4 or 8. Integer.
inline constexpr const char* kTabWidth = "tab_width";
inline constexpr int kTabWidthDefault = 4;

// [files]
inline constexpr const char* kFilesGroup = "files";

// Ask before File > Reload from Disk replaces a clean tab over 10 MB. Boolean.
inline constexpr const char* kConfirmLargeReload = "confirm_large_reload";
inline constexpr bool kConfirmLargeReloadDefault = true;

// Above this many characters on one line, opening a file asks first. Integer, kept between the
// two bounds.
inline constexpr const char* kLongLineLimit = "long_line_limit";
inline constexpr int kLongLineLimitDefault = 350000;
inline constexpr int kLongLineLimitMin = 10000;
inline constexpr int kLongLineLimitMax = 5000000;

} // namespace vermilion::settings
