// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line, the column part: which character of a line a typed column means.

#pragma once

#include <cstdint>
#include <string>

namespace vermilion {

// The column the status bar shows ("Ln 3, Col 7") counts characters from 1, and a tab counts as
// the columns it covers: it ends at the next multiple of tab_width. This function turns such a
// column back into a position in one line.
//
// line_text is one line of UTF-8 without its line break. The result is the number of characters
// that come before the typed column, so it is the character offset to put the cursor at:
//   - a column that falls on a character gives that character's offset;
//   - a column that falls inside a tab gives the offset of the tab (the cursor goes before it);
//   - a column past the end of the line, or the column just after the last character, gives the
//     number of characters of the line (the cursor goes to the end of the line);
//   - a column of 0 is treated as 1; a tab_width of 0 is treated as 1.
// A character is one code point (a continuation byte 0x80 to 0xBF does not start a new one);
// every character other than the tab covers one column.
std::uint64_t goto_column_offset(const std::string& line_text, unsigned tab_width,
                                 std::uint64_t column);

} // namespace vermilion
