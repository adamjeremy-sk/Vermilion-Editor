// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Parses the inline part of the manual's Markdown subset: bold, code, topic links and plain text.

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/manual_page.h"

namespace vermilion {

// A topic file name: 4 to 64 characters, lower-case letters, digits and hyphens followed by
// ".md", not starting with a hyphen ("open-and-save.md"). No slash, no other dot.
bool is_topic_name(std::string_view name);

// Parses one line of text (the content of a heading, a joined paragraph, a list item or a table
// cell) into spans, from left to right. Neighbouring plain text is merged into one Text span. A
// construct outside the subset stays in the text exactly as written and adds one to fallbacks.
//
//  - "**text**": Bold. Its text is taken literally (no markup inside) and must not be empty.
//    An unclosed "**" is plain text "**" and a fallback; "****" is plain text and one fallback.
//  - "`text`": Code. The next backtick ends it; the text must not be empty. An unclosed backtick
//    is plain text and a fallback; an empty pair "``" is plain text and one fallback.
//  - "[text](name.md)": Link when name.md satisfies is_topic_name and text is not empty. Its
//    text may be at most 200 bytes and may not contain "[" or "]"; its target at most 64 bytes
//    (the closing "]" and ")" are only looked for inside these limits). A complete
//    "[text](target)" that is not a topic link stays as written, with one fallback. A "[" with
//    no complete "[text](target)" after it is ordinary text with no fallback.
//  - "![text](target)" (an image) stays as written, with one fallback; a "!" with no complete
//    "[text](target)" after it is ordinary text.
//  - a "*" that is not part of "**" is plain text and a fallback; a "<" followed by a letter,
//    "/" or "!" is plain text and a fallback.
//  - everything else is plain text. A backslash has no special meaning.
//
// Hardening: the closing "]" and ")" of a link are only looked for inside the size limits above,
// so the time stays linear in the length of text, even for a megabyte of "[".
std::vector<ManualSpan> parse_spans(std::string_view text, std::size_t& fallbacks);

} // namespace vermilion
