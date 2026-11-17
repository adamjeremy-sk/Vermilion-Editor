// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search session's Replace All in slices (a run), no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <string>

#include "core/search_options.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"
#include "ui/search_session.h"

namespace {

using vermilion::SearchOptions;
using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::take_string;
using vermilion::ui::ReplaceAllResult;
using vermilion::ui::ReplaceSlice;
using vermilion::ui::SearchSession;

SearchOptions plain(const std::string& text) {
    SearchOptions options;
    options.text = text;
    return options;
}

SearchOptions regex_of(const std::string& text) {
    SearchOptions options;
    options.text = text;
    options.regex = true;
    return options;
}

class SearchSessionRunTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Replaces the text with an empty undo history and puts the cursor at the start.
    void set_text(const std::string& text) {
        gtk_text_buffer_begin_irreversible_action(text_buffer());
        gtk_text_buffer_set_text(text_buffer(), text.c_str(), -1);
        gtk_text_buffer_end_irreversible_action(text_buffer());
        select(0, 0);
    }

    std::string text() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_bounds(text_buffer(), &start, &end);
        return take_string(gtk_text_buffer_get_text(text_buffer(), &start, &end, TRUE));
    }

    // Selects the characters from offset `from` to offset `to` (the cursor ends at `to`).
    void select(int from, int to) {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &start, from);
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &end, to);
        gtk_text_buffer_select_range(text_buffer(), &end, &start);
    }

    int selection_start() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&start);
    }

    int selection_end() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&end);
    }

    bool can_undo() const { return gtk_text_buffer_get_can_undo(text_buffer()) != FALSE; }

    // `count` times "ab ", so the text has 3 * count characters and `count` matches of "ab".
    static std::string many_ab(int count) {
        std::string result;
        for (int i = 0; i < count; ++i) {
            result += "ab ";
        }
        return result;
    }

    ObjectPtr<GtkSourceBuffer> buffer_;
};

TEST_F(SearchSessionRunTest, ARunReplacesEveryMatchInSlices) {
    set_text("ab ab ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_FALSE(session.replace_run_active());
    ASSERT_TRUE(session.begin_replace_run("X", 0, 14));
    EXPECT_TRUE(session.replace_run_active());
    ReplaceSlice slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_FALSE(slice.finished);
    EXPECT_EQ(text(), "X X ab ab ab");
    slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_FALSE(slice.finished);
    EXPECT_EQ(text(), "X X X X ab");
    slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 1u);
    EXPECT_TRUE(slice.finished);
    const ReplaceAllResult result = session.end_replace_run();
    EXPECT_EQ(result.count, 5u);
    EXPECT_FALSE(session.replace_run_active());
    EXPECT_EQ(text(), "X X X X X");
}

TEST_F(SearchSessionRunTest, ASliceThatStopsAtTheLimitEndsWithAnEmptyFinishingSlice) {
    set_text("ab ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 11));
    ReplaceSlice slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_FALSE(slice.finished);
    slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_FALSE(slice.finished);
    slice = session.replace_run_slice(2, 0);
    EXPECT_EQ(slice.replaced, 0u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 4u);
    EXPECT_EQ(text(), "X X X X");
}

TEST_F(SearchSessionRunTest, TheWholeRunIsOneUndoStepAcrossIdleTurns) {
    set_text(many_ab(6));
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 18));
    int slices = 0;
    ReplaceSlice slice;
    while (!slice.finished && slices < 100) {
        slice = session.replace_run_slice(1, 0);
        ++slices;
        g_main_context_iteration(nullptr, FALSE);
    }
    EXPECT_EQ(slices, 7);
    EXPECT_EQ(session.end_replace_run().count, 6u);
    EXPECT_EQ(text(), "X X X X X X ");
    EXPECT_TRUE(can_undo());
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), many_ab(6));
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionRunTest, ARunInARangeLeavesTheRestAloneAndSelectsTheRange) {
    set_text("ab ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 3, 8));
    const ReplaceSlice slice = session.replace_run_slice(0, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 2u);
    EXPECT_EQ(text(), "ab X X ab");
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 6);
}

TEST_F(SearchSessionRunTest, AMatchThatCrossesTheEndOfTheRangeIsLeftAlone) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 4));
    const ReplaceSlice slice = session.replace_run_slice(0, 0);
    EXPECT_EQ(slice.replaced, 1u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 1u);
    EXPECT_EQ(text(), "X ab ab");
}

TEST_F(SearchSessionRunTest, ADeadlineThatHasPassedEndsASliceAfterSixteenMatches) {
    set_text(many_ab(40));
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 120));
    ReplaceSlice slice = session.replace_run_slice(0, 1);
    EXPECT_EQ(slice.replaced, 16u);
    EXPECT_FALSE(slice.finished);
    slice = session.replace_run_slice(0, 1);
    EXPECT_EQ(slice.replaced, 16u);
    EXPECT_FALSE(slice.finished);
    slice = session.replace_run_slice(0, 1);
    EXPECT_EQ(slice.replaced, 8u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 40u);
}

TEST_F(SearchSessionRunTest, NoLimitAndNoDeadlineFinishInOneSlice) {
    set_text(many_ab(40));
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 120));
    const ReplaceSlice slice = session.replace_run_slice(0, 0);
    EXPECT_EQ(slice.replaced, 40u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 40u);
}

TEST_F(SearchSessionRunTest, ManySlicesReplaceEverythingAndKeepTheRunOnTrack) {
    // One short line per match: a single line of 15,000 characters makes every edit slow.
    std::string lines;
    for (int i = 0; i < 5000; ++i) {
        lines += "ab\n";
    }
    set_text(lines);
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("Z", 0, 15000));
    std::uint64_t total = 0;
    int slices = 0;
    ReplaceSlice slice;
    while (!slice.finished && slices < 100) {
        slice = session.replace_run_slice(300, 0);
        total += slice.replaced;
        ++slices;
    }
    EXPECT_EQ(slices, 17);
    EXPECT_EQ(total, 5000u);
    EXPECT_EQ(session.end_replace_run().count, 5000u);
    EXPECT_EQ(text().size(), 10000u);
    EXPECT_EQ(text().substr(0, 6), "Z\nZ\nZ\n");
}

TEST_F(SearchSessionRunTest, AGrowingReplacementIsNotMatchedAgain) {
    set_text("e e");
    SearchSession session(buffer_.get());
    session.set_options(plain("e"));
    ASSERT_TRUE(session.begin_replace_run("ee", 0, 3));
    const ReplaceSlice slice = session.replace_run_slice(0, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_TRUE(slice.finished);
    EXPECT_EQ(session.end_replace_run().count, 2u);
    EXPECT_EQ(text(), "ee ee");
}

TEST_F(SearchSessionRunTest, GroupsWorkInARun) {
    set_text("a1 b2");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("([a-z])(\\d)"));
    ASSERT_TRUE(session.begin_replace_run("\\2\\1", 0, 5));
    const ReplaceSlice slice = session.replace_run_slice(0, 0);
    EXPECT_EQ(slice.replaced, 2u);
    EXPECT_EQ(session.end_replace_run().count, 2u);
    EXPECT_EQ(text(), "1a 2b");
}

TEST_F(SearchSessionRunTest, ARunIsRefusedForABadPatternAReplacementOrAnInactiveSearch) {
    set_text("a a");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("("));
    EXPECT_FALSE(session.begin_replace_run("x", 0, 3));
    session.set_options(regex_of("a"));
    EXPECT_FALSE(session.begin_replace_run("\\e", 0, 3));
    session.set_options(regex_of("(a)"));
    EXPECT_FALSE(session.begin_replace_run("\\2", 0, 3));
    session.set_options(plain(""));
    EXPECT_FALSE(session.begin_replace_run("x", 0, 3));
    EXPECT_FALSE(session.replace_run_active());
    EXPECT_EQ(text(), "a a");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionRunTest, AnEmptyRangeIsRefusedAndAnOutOfBoundsRangeIsClamped) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_FALSE(session.begin_replace_run("X", 3, 3));
    EXPECT_FALSE(session.begin_replace_run("X", 4, 1));
    EXPECT_FALSE(session.replace_run_active());
    ASSERT_TRUE(session.begin_replace_run("X", -5, 100));
    EXPECT_EQ(session.replace_run_slice(0, 0).replaced, 2u);
    EXPECT_EQ(session.end_replace_run().count, 2u);
    EXPECT_EQ(text(), "X X");
}

TEST_F(SearchSessionRunTest, ASecondRunCannotStartWhileOneIsActive) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    ASSERT_TRUE(session.begin_replace_run("X", 0, 5));
    EXPECT_FALSE(session.begin_replace_run("Y", 0, 5));
    EXPECT_EQ(session.replace_run_slice(0, 0).replaced, 2u);
    EXPECT_EQ(session.end_replace_run().count, 2u);
    EXPECT_EQ(text(), "X X");
    set_text("ab");
    ASSERT_TRUE(session.begin_replace_run("Y", 0, 2));
    EXPECT_EQ(session.replace_run_slice(0, 0).replaced, 1u);
    EXPECT_EQ(session.end_replace_run().count, 1u);
    EXPECT_EQ(text(), "Y");
}

TEST_F(SearchSessionRunTest, SlicesAndEndWithoutARunDoNothing) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    const ReplaceSlice slice = session.replace_run_slice(10, 0);
    EXPECT_EQ(slice.replaced, 0u);
    EXPECT_TRUE(slice.finished);
    const ReplaceAllResult result = session.end_replace_run();
    EXPECT_EQ(result.count, 0u);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionRunTest, DestroyingTheSessionDuringARunClosesTheUndoStep) {
    set_text("ab ab ab");
    {
        SearchSession session(buffer_.get());
        session.set_options(plain("ab"));
        ASSERT_TRUE(session.begin_replace_run("X", 0, 8));
        EXPECT_EQ(session.replace_run_slice(2, 0).replaced, 2u);
    }
    EXPECT_EQ(text(), "X X ab");
    EXPECT_TRUE(can_undo());
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_FALSE(can_undo());
}

} // namespace
