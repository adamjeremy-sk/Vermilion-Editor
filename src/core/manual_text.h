// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The text of a manual page exactly as the Help window shows it, with the ranges to style.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/manual_page.h"

namespace vermilion {

// The width of the Help page in characters. Tables are laid out to fit it; other lines are
// wrapped by the view.
inline constexpr std::size_t kManualColumns = 100;

// Every offset and width in this file counts characters (Unicode code points), not bytes: the
// unit of GtkTextBuffer offsets. The Help page is monospace, so one character is one column
// (combining marks and wide characters are not handled).
//
// The number of characters in valid UTF-8 text: the bytes that are not continuation bytes (those
// of the form 10xxxxxx). Invalid text never fails; it is counted the same way.
std::size_t character_count(std::string_view text);

// A stretch of the text from `begin` up to but not including `end`; never empty.
struct TextRange {
    std::size_t begin = 0;
    std::size_t end = 0;
};

// A topic link: the range of its words and the topic file it opens ("open-and-save.md").
struct LinkRange {
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string target;
};

// A heading: the range of its words, its level (1 to 6) and its words as plain text.
struct HeadingMark {
    std::size_t begin = 0;
    std::size_t end = 0;
    int level = 1;
    std::string title;
};

// The page as one string and what the view must style. Every vector is in increasing order of
// begin, and its ranges do not overlap (ranges of two different vectors may).
struct ManualText {
    std::string text;
    std::size_t length = 0;               // character_count(text)
    std::vector<TextRange> bold;          // bold spans, headings included
    std::vector<TextRange> code;          // code spans
    std::vector<LinkRange> links;         // topic links
    std::vector<HeadingMark> headings;    // one per heading block, in page order
    std::vector<TextRange> list_items;    // each item from its bullet to the end of its text
    std::vector<TextRange> tables;        // each table from its first to its last character
    std::vector<TextRange> table_headers; // the header row of each table (all its lines)
};

// Lays out `page`:
//  - Blocks follow each other with one empty line between them, except that list items follow
//    each other on the next line. There is no newline before the first block or after the last;
//    a page without blocks gives empty text.
//  - Heading: its text on one line (the view wraps a long one). A heading or paragraph whose text
//    is empty is skipped.
//  - Paragraph: its text on one line (the view wraps it).
//  - List item: U+2022 and a space, then its text on one line. `list_items` covers all of it, so
//    the view can indent a wrapped item by two characters.
//  - Table: see "Tables" below; the layout code is in core/manual_table.h.
//  Spans give their text in order; Bold, Code and Link spans (with text) add to `bold`, `code` and
//  `links`; Text spans add nothing.
//
// Tables (a table with no rows or no columns is skipped). The column count is the larger of
// `columns` and the longest row; a short row gets empty cells. Cell text is split into words at
// spaces and tabs, so runs of blanks become one space, and is wrapped inside its column (a word
// longer than the column is cut into pieces of the column's width). The widths come from
// fit_columns (core/manual_table.h) with the natural width of a column (its widest cell on one
// line) and its longest word. Every line of the table has the same length: each cell is padded with
// spaces to its column's width, and columns are joined by " | ". A row takes as many lines as its
// tallest cell. Right after the header row comes a line of "-" under each column joined by "-+-".
// Styled spans keep their style over a wrap; the padding and the separators have none.
// `table_headers` covers the lines of the header row and not the line of dashes.
ManualText build_manual_text(const ManualPage& page);

// The link at character offset `offset`, or null if there is none there: a link covers its
// `begin` up to but not including its `end`. For clicks in the view.
const LinkRange* link_at(const ManualText& built, std::size_t offset);

} // namespace vermilion
