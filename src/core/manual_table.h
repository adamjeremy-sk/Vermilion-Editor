// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Lays out a manual table as padded text: column widths, wrapped cells, the ranges to style.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_text.h"

namespace vermilion {

// The column widths for a table whose line must fit in `total` characters. `natural[c]` is the
// width of column c with no wrapping and `minimum[c]` the length of its longest word; both are
// raised to at least 1 and `minimum` is cut to `natural` where it is larger. The columns are
// joined by 3 characters each, so `available` is total minus 3 for every gap.
//  - If the natural widths fit in `available`, they are the widths.
//  - Otherwise every column starts at 1 and the spare characters (available minus the column
//    count) are handed out one at a time, each to the column furthest below its target, the
//    leftmost on a tie: first towards the minimums, then towards the natural widths, until the
//    spare characters or the deficits run out. Words that are longer than their column are cut
//    only when `available` cannot hold every minimum.
//  - If `available` is not larger than the column count, every width is 1 (the line is then
//    longer than `total`).
// Both vectors have the column count of elements; an empty input gives an empty result.
std::vector<std::size_t> fit_columns(const std::vector<std::size_t>& natural,
                                     const std::vector<std::size_t>& minimum, std::size_t total);

// A table laid out as text. Offsets count characters from the start of `text`.
struct TableLayout {
    std::string text;           // every line, with no newline after the last one
    std::size_t length = 0;     // character_count(text)
    std::size_t header_end = 0; // where the last line of the header row ends
    std::vector<TextRange> bold;
    std::vector<TextRange> code;
    std::vector<LinkRange> links;
};

// Lays out `table` in lines of `total` characters (the rules are in core/manual_text.h, "Tables").
// The caller has checked that the table has a row and a column.
TableLayout layout_table(const ManualBlock& table, std::size_t total);

// The narrowest `total` at which layout_table cuts no word of `table`: the longest word of each
// column plus 3 characters for every gap between columns. 0 for a table with no column.
std::size_t table_min_columns(const ManualBlock& table);

// The largest table_min_columns of the tables of `page` that have a row, or 0 when there is none.
// The Help page never lays a table out in less, and scrolls sideways when its window is narrower.
std::size_t page_min_columns(const ManualPage& page);

} // namespace vermilion
