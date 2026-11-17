// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line without a window: counting the lines of a buffer and moving the cursor to a line.

#include "ui/line_jump.h"

#include <gtk/gtk.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/goto_column.h"
#include "core/goto_line.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

std::string line_text(GtkTextBuffer* buffer, const GtkTextIter& start) {
    GtkTextIter end = start;
    if (gtk_text_iter_ends_line(&end) == FALSE) {
        gtk_text_iter_forward_to_line_end(&end);
    }
    return gtkx::take_string(gtk_text_buffer_get_text(buffer, &start, &end, FALSE));
}

} // namespace

std::uint64_t line_count_of(GtkTextBuffer* buffer) {
    const int count = gtk_text_buffer_get_line_count(buffer);
    return count < 1 ? 1 : static_cast<std::uint64_t>(count);
}

std::string range_text_of(GtkTextBuffer* buffer) {
    return goto_range_text(line_count_of(buffer));
}

std::uint64_t current_line_of(GtkTextBuffer* buffer) {
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, gtk_text_buffer_get_insert(buffer));
    return static_cast<std::uint64_t>(gtk_text_iter_get_line(&cursor)) + 1;
}

JumpResult jump_to_line(GtkTextBuffer* buffer, unsigned tab_width, const std::string& typed) {
    const std::optional<GotoRequest> request = parse_goto(typed);
    if (!request.has_value()) {
        JumpResult result;
        result.kind = JumpKind::Invalid;
        result.message = kJumpHint;
        return result;
    }
    const GotoTarget target = resolve_goto(*request, line_count_of(buffer));
    GtkTextIter position;
    gtk_text_buffer_get_iter_at_line(buffer, &position, static_cast<int>(target.line - 1));
    if (target.column > 0) {
        const std::uint64_t offset =
            goto_column_offset(line_text(buffer, position), tab_width, target.column);
        gtk_text_iter_forward_chars(&position, static_cast<int>(offset));
    }
    gtk_text_buffer_place_cursor(buffer, &position);
    JumpResult result;
    if (target.past_end) {
        result.kind = JumpKind::PastEnd;
        result.message = goto_past_end_message(*request, target.line);
    } else {
        result.kind = JumpKind::Jumped;
    }
    return result;
}

} // namespace vermilion::ui
