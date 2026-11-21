// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The CSS rule that gives the editor text its font.

#pragma once

#include <string>

namespace vermilion::ui {

// The CSS class that the text view of every editor page carries; the rule below is for it.
inline constexpr const char* kEditorFontClass = "vm-editor";

// The stylesheet text for the font that `description` names, a Pango font description such as
// "DejaVu Sans Mono 11" or "Sans Bold Italic 10.5": one rule for the class vm-editor with only the
// properties that differ from the plain text, in this order (Pango always reports a weight and a
// style, "normal" when the text names none, so those two are written only when they are not
// normal):
//   font-family: every name of the (comma separated) family list, in double quotes,
//                with a backslash or a double quote escaped by a backslash and every character
//                below U+0020 turned into a space; empty names are left out
//   font-size:   the size in points ("11pt", "10.5pt": at most two decimals, no trailing zeros,
//                always a dot), or in pixels ("12px") when the description gives an absolute size;
//                a size of zero is left out
//   font-weight: the Pango weight as a number ("700"), when it is not normal (400)
//   font-style:  "oblique" or "italic", when the style is not normal
// Each property is on its own line, indented by four spaces, and the text ends with a newline
// after the closing brace. Empty or blank text, and a description with nothing to write, give the
// empty text: no rule, so the system monospace font stays. Pure; needs no display and no main
// loop.
std::string font_css(const std::string& description);

} // namespace vermilion::ui
