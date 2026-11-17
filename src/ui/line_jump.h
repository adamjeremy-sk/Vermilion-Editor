// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line without a window: counting the lines of a buffer and moving the cursor to a line.

#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <string>

namespace vermilion::ui {

// What the window shows when the typed text is not a line number or line:column.
inline constexpr char kJumpHint[] = "Type a line number, or line:column, for example 120:8.";

enum class JumpKind {
    Invalid, // the typed text was not understood; nothing moved
    Jumped,  // the cursor is on the line that was asked for
    PastEnd  // the line does not exist; the cursor is at the start of the last line
};

struct JumpResult {
    JumpKind kind = JumpKind::Invalid;
    // Invalid: kJumpHint. PastEnd: goto_past_end_message() ("Line 99,999 does not exist; went to
    // the last line, 12,408."). Jumped: empty.
    std::string message;
};

// The number of lines of the buffer, as the status bar counts them: a buffer that ends with a line
// break has one more, empty, line after it. Never less than 1. `buffer` must not be null.
std::uint64_t line_count_of(GtkTextBuffer* buffer);

// The range the Go to Line window shows: goto_range_text(line_count_of(buffer)), "1 to 12,408".
std::string range_text_of(GtkTextBuffer* buffer);

// The line of the cursor (the insert mark), counted from 1 like the status bar's "Ln".
std::uint64_t current_line_of(GtkTextBuffer* buffer);

// Reads `typed` with parse_goto() and moves the cursor of `buffer` with place_cursor(), which
// also drops any selection. Nothing else in the buffer changes (not the text, not the modified
// flag). `tab_width` is the document's tab width: a typed column counts a tab as the columns it
// covers, like the status bar's "Col" (core/goto_column.h).
//   - Not understood: Invalid, the cursor and the selection stay as they were.
//   - A line inside the buffer: Jumped; the cursor goes to that line, at the typed column fitted
//     with goto_column_offset(), or at the start of the line when no column was typed.
//   - A line beyond the last: PastEnd; the cursor goes to the start of the last line and any
//     typed column is ignored.
// The caller scrolls the view; this function knows no view. `buffer` must not be null.
JumpResult jump_to_line(GtkTextBuffer* buffer, unsigned tab_width, const std::string& typed);

} // namespace vermilion::ui
