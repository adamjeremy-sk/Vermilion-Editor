// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for Go to Line without a window: counting lines and moving the cursor, no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"
#include "ui/line_jump.h"

namespace {

using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::take_string;
using vermilion::ui::current_line_of;
using vermilion::ui::jump_to_line;
using vermilion::ui::JumpKind;
using vermilion::ui::JumpResult;
using vermilion::ui::kJumpHint;
using vermilion::ui::line_count_of;
using vermilion::ui::range_text_of;

// Lines: "alpha" (offsets 0 to 4), "beta" (6 to 9), "<tab>gamma" (11 to 16), and an empty line
// after the last line break (offset 18).
constexpr char kText[] = "alpha\nbeta\n\tgamma\n";

class LineJumpTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Puts `text` into the buffer with the cursor at the start.
    void open(const char* text) {
        gtk_text_buffer_set_text(text_buffer(), text, -1);
        put_cursor(0);
    }

    void put_cursor(int offset) {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &iter, offset);
        gtk_text_buffer_place_cursor(text_buffer(), &iter);
    }

    void select(int from, int to) {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &start, from);
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &end, to);
        gtk_text_buffer_select_range(text_buffer(), &end, &start);
    }

    GtkTextIter cursor() const {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(text_buffer(), &iter,
                                         gtk_text_buffer_get_insert(text_buffer()));
        return iter;
    }

    // The cursor's line counted from 0, and its position in the line in characters.
    int cursor_line() const {
        GtkTextIter iter = cursor();
        return gtk_text_iter_get_line(&iter);
    }
    int cursor_column() const {
        GtkTextIter iter = cursor();
        return gtk_text_iter_get_line_offset(&iter);
    }
    int cursor_offset() const {
        GtkTextIter iter = cursor();
        return gtk_text_iter_get_offset(&iter);
    }

    bool has_selection() const {
        GtkTextIter start;
        GtkTextIter end;
        return gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end) != FALSE;
    }

    std::string text() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_bounds(text_buffer(), &start, &end);
        return take_string(gtk_text_buffer_get_text(text_buffer(), &start, &end, TRUE));
    }

    JumpResult jump(const std::string& typed, unsigned tab_width = 4) {
        return jump_to_line(text_buffer(), tab_width, typed);
    }

    ObjectPtr<GtkSourceBuffer> buffer_;
};

TEST_F(LineJumpTest, AnEmptyBufferHasOneLine) {
    open("");
    EXPECT_EQ(line_count_of(text_buffer()), 1u);
}

TEST_F(LineJumpTest, ALineBreakAtTheEndStartsAnotherLine) {
    open(kText);
    EXPECT_EQ(line_count_of(text_buffer()), 4u);
    open("a\nb");
    EXPECT_EQ(line_count_of(text_buffer()), 2u);
}

TEST_F(LineJumpTest, TheRangeTextUsesCommas) {
    open(kText);
    EXPECT_EQ(range_text_of(text_buffer()), "1 to 4");
    open(std::string(12407, '\n').c_str());
    EXPECT_EQ(range_text_of(text_buffer()), "1 to 12,408");
}

TEST_F(LineJumpTest, TheCurrentLineCountsFromOne) {
    open(kText);
    EXPECT_EQ(current_line_of(text_buffer()), 1u);
    put_cursor(7);
    EXPECT_EQ(current_line_of(text_buffer()), 2u);
    put_cursor(18);
    EXPECT_EQ(current_line_of(text_buffer()), 4u);
}

TEST_F(LineJumpTest, JumpsToTheStartOfALine) {
    open(kText);
    JumpResult result = jump("3");
    EXPECT_EQ(result.kind, JumpKind::Jumped);
    EXPECT_EQ(result.message, "");
    EXPECT_EQ(cursor_line(), 2);
    EXPECT_EQ(cursor_column(), 0);
}

TEST_F(LineJumpTest, JumpsToALineAndColumn) {
    open(kText);
    EXPECT_EQ(jump("1:3").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 0);
    EXPECT_EQ(cursor_column(), 2);
}

TEST_F(LineJumpTest, AColumnPastTheEndOfTheLineGoesToTheEndOfThatLine) {
    open(kText);
    EXPECT_EQ(jump("2:50").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 1);
    EXPECT_EQ(cursor_column(), 4);
}

TEST_F(LineJumpTest, AColumnCountsATabAsTheColumnsItCovers) {
    open(kText); // line 3 is a tab and "gamma"
    EXPECT_EQ(jump("3:5").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 2);
    EXPECT_EQ(cursor_column(), 1);
    jump("3:3");
    EXPECT_EQ(cursor_column(), 0);
    jump("3:6");
    EXPECT_EQ(cursor_column(), 2);
    jump("3:9", 8);
    EXPECT_EQ(cursor_column(), 1);
}

TEST_F(LineJumpTest, AColumnCountsCharactersNotBytes) {
    open("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\nabc");
    EXPECT_EQ(jump("1:3").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 0);
    EXPECT_EQ(cursor_column(), 2);
    jump("1:4");
    EXPECT_EQ(cursor_column(), 3);
    jump("2:2");
    EXPECT_EQ(cursor_line(), 1);
    EXPECT_EQ(cursor_column(), 1);
}

TEST_F(LineJumpTest, TheLastLineIsNotPastTheEnd) {
    open("a\nb");
    JumpResult result = jump("2");
    EXPECT_EQ(result.kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 1);
    open(kText);
    EXPECT_EQ(jump("4:5").kind, JumpKind::Jumped); // the empty line after the last line break
    EXPECT_EQ(cursor_line(), 3);
    EXPECT_EQ(cursor_column(), 0);
}

TEST_F(LineJumpTest, ALineBeyondTheEndGoesToTheStartOfTheLastLine) {
    open(kText);
    JumpResult result = jump("99");
    EXPECT_EQ(result.kind, JumpKind::PastEnd);
    EXPECT_EQ(result.message, "Line 99 does not exist; went to the last line, 4.");
    EXPECT_EQ(cursor_line(), 3);
    EXPECT_EQ(cursor_column(), 0);
}

TEST_F(LineJumpTest, ALineBeyondTheEndIgnoresTheColumn) {
    open("a\nbcdef");
    JumpResult result = jump("9:4");
    EXPECT_EQ(result.kind, JumpKind::PastEnd);
    EXPECT_EQ(result.message, "Line 9 does not exist; went to the last line, 2.");
    EXPECT_EQ(cursor_line(), 1);
    EXPECT_EQ(cursor_column(), 0);
}

TEST_F(LineJumpTest, ALineInAnEmptyBufferIsLineOne) {
    open("");
    EXPECT_EQ(jump("1").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_offset(), 0);
}

TEST_F(LineJumpTest, SpacesAroundTheTextAreIgnored) {
    open(kText);
    EXPECT_EQ(jump("  2  ").kind, JumpKind::Jumped);
    EXPECT_EQ(cursor_line(), 1);
}

TEST_F(LineJumpTest, TextThatIsNotALineIsRefusedAndNothingMoves) {
    open(kText);
    put_cursor(3);
    const char* const refused[] = {"", " ", "abc", "0", "5:", "1:0", "-2", "1 2"};
    for (const char* typed : refused) {
        JumpResult result = jump(typed);
        EXPECT_EQ(result.kind, JumpKind::Invalid);
        EXPECT_EQ(result.message, std::string(kJumpHint));
        EXPECT_EQ(cursor_offset(), 3);
    }
}

TEST_F(LineJumpTest, RefusedTextKeepsTheSelection) {
    open(kText);
    select(1, 4);
    EXPECT_EQ(jump("x").kind, JumpKind::Invalid);
    EXPECT_TRUE(has_selection());
}

TEST_F(LineJumpTest, AJumpDropsTheSelectionAndLeavesTheTextAlone) {
    open(kText);
    select(1, 4);
    EXPECT_EQ(jump("2").kind, JumpKind::Jumped);
    EXPECT_FALSE(has_selection());
    EXPECT_EQ(text(), std::string(kText));
}

} // namespace
