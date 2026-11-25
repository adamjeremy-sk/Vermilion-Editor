// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the editor text looks like apart from the stylesheet: scheme, current line, tab, font.

#pragma once

#include <string>

#include "core/editor_settings.h"

namespace vermilion {

// The part of the settings that every editor page shows. The colour mode is not in it: it has
// been turned into `dark` (see resolve_dark in core/colour_mode.h).
struct EditorLook {
    // Which editor scheme: the dark one or the light one.
    bool dark = false;
    // The band under the line with the cursor.
    bool highlight_current_line = settings::kHighlightCurrentLineDefault;
    // 4 or 8: how wide a tab is drawn, and how many spaces the Tab key inserts.
    int tab_width = settings::kTabWidthDefault;
    // A Pango font description; empty text means the system monospace font.
    std::string font = settings::kFontDefault;
};

inline bool operator==(const EditorLook& a, const EditorLook& b) {
    return a.dark == b.dark && a.highlight_current_line == b.highlight_current_line &&
           a.tab_width == b.tab_width && a.font == b.font;
}

inline bool operator!=(const EditorLook& a, const EditorLook& b) {
    return !(a == b);
}

// The look for `values` and the decision `dark`. The values are normalised first
// (normalise_settings in core/editor_settings.h), so the tab width is 4 or 8 and the font has no
// blanks around it.
inline EditorLook make_look(const EditorSettings& values, bool dark) {
    const EditorSettings clean = normalise_settings(values);
    EditorLook look;
    look.dark = dark;
    look.highlight_current_line = clean.highlight_current_line;
    look.tab_width = clean.tab_width;
    look.font = clean.font;
    return look;
}

} // namespace vermilion
