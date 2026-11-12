// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Parses the Markdown subset of the user manual (docs/manual/*.md) into blocks and spans.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace vermilion {

enum class SpanKind { Text, Bold, Code, Link };

struct ManualSpan {
    SpanKind kind = SpanKind::Text;
    std::string text;   // what is shown
    std::string target; // Link only: the topic file it opens, for example "open-and-save.md"
};

using ManualCell = std::vector<ManualSpan>;
using ManualRow = std::vector<ManualCell>;

enum class ManualBlockKind { Heading, Paragraph, ListItem, Table };

struct ManualBlock {
    ManualBlockKind kind = ManualBlockKind::Paragraph;
    int level = 0;                 // Heading: 1 to 6
    std::vector<ManualSpan> spans; // Heading, Paragraph and ListItem
    std::vector<ManualRow> rows;   // Table: rows[0] is the header row; rows may differ in length
    std::size_t columns = 0;       // Table: the cell count of the widest row
};

struct ManualPage {
    std::vector<ManualBlock> blocks;
    // How many constructs outside the subset were met (block level and span level together).
    // Each one is shown as plain text, nothing is dropped; a clean manual page has none.
    std::size_t fallbacks = 0;
};

// Parses the text of one manual page. The text is bytes: valid UTF-8 is expected and passes
// through untouched. Never fails; time and memory grow linearly with the size of the text.
//
// Lines end at "\n"; one "\r" before it is ignored. Blank lines (empty or only spaces and tabs)
// separate blocks. Every text that becomes a heading, paragraph, list item or table cell is
// trimmed (spaces and tabs) and goes through parse_spans (core/manual_spans.h).
//
// Blocks
//  - Heading: a line starting at the left edge with 1 to 6 "#", then at least one space or tab,
//    then text that is not empty ("#tag", "#" alone and " # x" are paragraphs).
//  - List item: a line starting at the left edge with "-", a space and text. The following lines
//    indented by two or more spaces belong to the item; all lines are trimmed and joined with
//    one space. Any other line ends the item. A continuation line whose text starts with "- " is
//    a nested list, which is outside the subset: it is joined as plain text and counts as one
//    fallback.
//  - Table: a line starting with "|" (after trimming) whose next line is a separator row with
//    the same number of cells, then every following line starting with "|". A row loses its
//    leading "|" and one trailing "|", is split at every "|" and each cell is trimmed. A
//    separator cell is dashes with an optional colon at either end ("---", ":--", "-:", ":-:").
//    A body row with a different number of cells than the header is kept as written and counts
//    as one fallback. A line starting with "|" that does not open a table is a paragraph and
//    counts as one fallback.
//  - Paragraph: any other line, and the following lines up to a blank line or to a line that
//    starts a heading, a list item or a table row ("|"). The trimmed lines are joined with one
//    space.
//
// Outside the subset, per paragraph line (one fallback each, the text stays as written): a line
// starting (after trimming) with ">" (quote), "```" or "~~~" (code fence), "+ ", digits followed
// by "." or ")" and then a space or the end of the line (numbered list), or consisting of three
// or more "-" and nothing else (rule).
ManualPage parse_manual(std::string_view markdown);

} // namespace vermilion
