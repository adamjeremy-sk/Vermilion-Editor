// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search session's Replace, and for Replace All run to the end in one go, no display.

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
using vermilion::ui::ReplaceResult;
using vermilion::ui::SearchSession;

SearchOptions plain(const std::string& text, bool match_case = false, bool whole_word = false) {
    SearchOptions options;
    options.text = text;
    options.match_case = match_case;
    options.whole_word = whole_word;
    return options;
}

SearchOptions regex_of(const std::string& text) {
    SearchOptions options;
    options.text = text;
    options.regex = true;
    return options;
}

// Runs the main loop until the session counts `expected` matches, for at most five seconds.
void wait_for_count(const SearchSession& session, int expected) {
    const gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (session.count() != expected && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

class SearchSessionReplaceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Replaces the text with an empty undo history and puts the cursor at the start.
    void set_text(const char* text) {
        gtk_text_buffer_begin_irreversible_action(text_buffer());
        gtk_text_buffer_set_text(text_buffer(), text, -1);
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

    // Replace All over the characters from offset `from` up to offset `to`, as the controller's
    // job does it but all at once: one run, slices until nothing is left, then the end of the run.
    // Returns the number of matches replaced; 0 when the run is refused.
    static std::uint64_t replace_range(SearchSession& session, const std::string& replacement,
                                       int from, int to) {
        if (!session.begin_replace_run(replacement, from, to)) {
            return 0;
        }
        for (int i = 0; i < 10000 && !session.replace_run_slice(0, 0).finished; ++i) {
        }
        return session.end_replace_run().count;
    }

    // The same over the whole text.
    std::uint64_t replace_whole(SearchSession& session, const std::string& replacement) const {
        return replace_range(session, replacement, 0,
                             gtk_text_buffer_get_char_count(text_buffer()));
    }

    ObjectPtr<GtkSourceBuffer> buffer_;
};

// No wait for the scan in the first tests: Replace has to work before the count is known.
TEST_F(SearchSessionReplaceTest, ReplaceReplacesTheSelectedMatchAndSelectsTheNext) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(0, 2);
    const ReplaceResult result = session.replace("X");
    EXPECT_FALSE(result.refused);
    EXPECT_TRUE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_FALSE(result.next.wrapped);
    EXPECT_EQ(text(), "X ab ab");
    EXPECT_EQ(selection_start(), 2);
    EXPECT_EQ(selection_end(), 4);
}

TEST_F(SearchSessionReplaceTest, ReplaceOnANonMatchOnlySelectsTheNext) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(1, 2);
    ReplaceResult result = session.replace("X");
    EXPECT_FALSE(result.refused);
    EXPECT_FALSE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 5);
    select(0, 0);
    result = session.replace("X");
    EXPECT_FALSE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_EQ(selection_start(), 0);
    EXPECT_EQ(selection_end(), 2);
}

TEST_F(SearchSessionReplaceTest, ReplaceOfTheLastMatchWrapsToTheFirst) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(6, 8);
    const ReplaceResult result = session.replace("X");
    EXPECT_TRUE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_TRUE(result.next.wrapped);
    EXPECT_EQ(text(), "ab ab X");
    EXPECT_EQ(selection_start(), 0);
    EXPECT_EQ(selection_end(), 2);
}

TEST_F(SearchSessionReplaceTest, ReplaceOfTheOnlyMatchLeavesTheCursorAfterIt) {
    set_text("xx ab xx");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(3, 5);
    const ReplaceResult result = session.replace("cd");
    EXPECT_TRUE(result.replaced);
    EXPECT_FALSE(result.next.found);
    EXPECT_FALSE(result.next.wrapped);
    EXPECT_EQ(text(), "xx cd xx");
    EXPECT_EQ(selection_start(), 5);
    EXPECT_EQ(selection_end(), 5);
}

TEST_F(SearchSessionReplaceTest, EachReplaceIsOneUndoStep) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(0, 2);
    EXPECT_FALSE(can_undo());
    EXPECT_TRUE(session.replace("X").replaced);
    EXPECT_TRUE(session.replace("X").replaced);
    EXPECT_EQ(text(), "X X ab");
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "X ab ab");
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionReplaceTest, ReplaceUsesGroupsWhenRegularExpressionIsOn) {
    set_text("a1 b2");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("([a-z])(\\d)"));
    select(0, 2);
    const ReplaceResult result = session.replace("\\2\\1");
    EXPECT_TRUE(result.replaced);
    EXPECT_EQ(text(), "1a b2");
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 5);
}

TEST_F(SearchSessionReplaceTest, ReplaceIsLiteralWhenRegularExpressionIsOff) {
    set_text("a.b");
    SearchSession session(buffer_.get());
    session.set_options(plain("."));
    select(1, 2);
    EXPECT_TRUE(session.replace("\\1\\n").replaced);
    EXPECT_EQ(text(), "a\\1\\nb");
}

TEST_F(SearchSessionReplaceTest, ReplaceIsRefusedForABadPatternOrReplacement) {
    set_text("a a");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("("));
    ReplaceResult result = session.replace("x");
    EXPECT_TRUE(result.refused);
    EXPECT_FALSE(result.replaced);
    EXPECT_FALSE(result.next.found);
    session.set_options(regex_of("a"));
    select(0, 1);
    result = session.replace("\\e");
    EXPECT_TRUE(result.refused);
    EXPECT_FALSE(result.replaced);
    session.set_options(regex_of("(a)"));
    result = session.replace("\\2");
    EXPECT_TRUE(result.refused);
    EXPECT_FALSE(result.replaced);
    EXPECT_EQ(text(), "a a");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionReplaceTest, ReplaceAllReplacesEveryMatchInOneUndoStep) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_EQ(replace_whole(session, "X"), 3u);
    EXPECT_EQ(text(), "X X X");
    EXPECT_TRUE(can_undo());
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionReplaceTest, ReplaceAllKeepsMatchCaseAndWholeWord) {
    set_text("Abc abc abcd ABC");
    SearchSession session(buffer_.get());
    session.set_options(plain("abc", false, true));
    EXPECT_EQ(replace_whole(session, "X"), 3u);
    EXPECT_EQ(text(), "X X abcd X");
    set_text("Abc abc abcd ABC");
    session.set_options(plain("abc", true, false));
    EXPECT_EQ(replace_whole(session, "X"), 2u);
    EXPECT_EQ(text(), "Abc X Xd ABC");
}

TEST_F(SearchSessionReplaceTest, ReplaceAllWithGroupsAndWithAnEmptyReplacement) {
    set_text("a1 b2 c3");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("(\\w)(\\d)"));
    EXPECT_EQ(replace_whole(session, "\\2\\1"), 3u);
    EXPECT_EQ(text(), "1a 2b 3c");
    set_text("a-b-c");
    session.set_options(plain("-"));
    EXPECT_EQ(replace_whole(session, ""), 2u);
    EXPECT_EQ(text(), "abc");
}

TEST_F(SearchSessionReplaceTest, ReplaceAllWithNothingToReplaceChangesNothing) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("zz"));
    EXPECT_EQ(replace_whole(session, "X"), 0u);
    session.set_options(plain(""));
    EXPECT_EQ(replace_whole(session, "X"), 0u);
    EXPECT_EQ(replace_range(session, "X", 0, 5), 0u);
    EXPECT_EQ(text(), "ab ab");
}

TEST_F(SearchSessionReplaceTest, TheCountFollowsReplaceAll) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    wait_for_count(session, 2);
    EXPECT_EQ(session.count(), 2);
    EXPECT_EQ(replace_whole(session, "X"), 2u);
    wait_for_count(session, 0);
    EXPECT_EQ(session.count(), 0);
}

TEST_F(SearchSessionReplaceTest, TheReplacementIsNotSearchedAgain) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_EQ(replace_whole(session, "abab"), 3u);
    EXPECT_EQ(text(), "abab abab abab");
    set_text("ab ab ab");
    EXPECT_EQ(replace_range(session, "abab", 0, 5), 2u);
    EXPECT_EQ(text(), "abab abab ab");
}

TEST_F(SearchSessionReplaceTest, ReplaceInRangeOnlyTouchesMatchesWhollyInsideTheRange) {
    set_text("ab ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_EQ(replace_range(session, "X", 3, 8), 2u);
    EXPECT_EQ(text(), "ab X X ab");
    set_text("ab ab ab");
    EXPECT_EQ(replace_range(session, "X", 0, 4), 1u);
    EXPECT_EQ(text(), "X ab ab");
    set_text("ab ab ab");
    EXPECT_EQ(replace_range(session, "X", 1, 8), 2u);
    EXPECT_EQ(text(), "ab X X");
}

TEST_F(SearchSessionReplaceTest, ReplaceInRangeIsOneUndoStepAndSelectsTheRange) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_EQ(replace_range(session, "XYZ", 0, 5), 2u);
    EXPECT_EQ(text(), "XYZ XYZ ab");
    EXPECT_EQ(selection_start(), 0);
    EXPECT_EQ(selection_end(), 7);
    EXPECT_TRUE(can_undo());
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(SearchSessionReplaceTest, ReplaceInRangeClampsItsOffsets) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_EQ(replace_range(session, "X", 3, 3), 0u);
    EXPECT_EQ(replace_range(session, "X", 5, 2), 0u);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_EQ(replace_range(session, "X", -5, 100), 2u);
    EXPECT_EQ(text(), "X X");
}

TEST_F(SearchSessionReplaceTest, ReplaceInRangeSkipsEmptyMatches) {
    set_text("a xx b");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("x*"));
    EXPECT_EQ(replace_range(session, "Y", 0, 6), 1u);
    EXPECT_EQ(text(), "a Y b");
}

} // namespace
