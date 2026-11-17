// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line: reading what was typed, fitting it to the document, and the texts around it.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vermilion {

// What the user typed, counted from 1 as people count lines and columns.
struct GotoRequest {
    std::uint64_t line = 1;
    std::uint64_t column = 0; // 0: no column was typed
};

// Reads "120" or "120:8". Spaces and tabs around the whole text are ignored; inside, only ASCII
// digits, and for the second form one colon with no space beside it. The line and the column
// must be at least 1 ("0", "5:0", "", "abc", "-3", "+3", "1.5", ":5", "5:", "1:2:3" and "1 2" are
// refused). Leading zeros are allowed ("007" is 7). A number above 999999999 is read as 999999999,
// so a very long run of digits never overflows.
std::optional<GotoRequest> parse_goto(const std::string& text);

// Where the cursor goes.
struct GotoTarget {
    std::uint64_t line = 1;
    std::uint64_t column = 0;
    bool past_end = false; // the typed line is beyond the last line
};

// Fits a request to a document of line_count lines (a count of 0 is treated as 1). A line inside
// the document is kept, with its column. A line beyond the last line becomes the last line, with
// past_end set and no column (0), so the cursor goes to the start of that line. The column itself
// is fitted to the length of the line by the caller, which can see the text.
GotoTarget resolve_goto(const GotoRequest& request, std::uint64_t line_count);

// The allowed range shown in the window: "1 to 12,408" (counts through with_commas()). A
// line_count of 0 is treated as 1.
std::string goto_range_text(std::uint64_t line_count);

// The status message after a jump past the end:
// "Line 99,999 does not exist; went to the last line, 12,408."
std::string goto_past_end_message(const GotoRequest& request, std::uint64_t last_line);

} // namespace vermilion
