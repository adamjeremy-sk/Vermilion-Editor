// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search session: counts, Find Next and Previous, on a real buffer with no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <string>

#include "core/search_options.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/search_session.h"

namespace {

using vermilion::SearchOptions;
using vermilion::gtkx::ObjectPtr;
using vermilion::ui::FindResult;
using vermilion::ui::SearchSession;

SearchOptions plain(const std::string& text, bool match_case = false, bool whole_word = false) {
    SearchOptions options;
    options.text = text;
    options.match_case = match_case;
    options.whole_word = whole_word;
    return options;
}

SearchOptions regex_of(const std::string& text, bool whole_word = false) {
    SearchOptions options;
    options.text = text;
    options.regex = true;
    options.whole_word = whole_word;
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

class SearchSessionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Replaces the text and puts the cursor at the start, with nothing selected.
    void set_text(const char* text) {
        gtk_text_buffer_set_text(text_buffer(), text, -1);
        select(0, 0);
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

    int cursor() const {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(text_buffer(), &iter,
                                         gtk_text_buffer_get_insert(text_buffer()));
        return gtk_text_iter_get_offset(&iter);
    }

    ObjectPtr<GtkSourceBuffer> buffer_;
};

TEST_F(SearchSessionTest, ASessionWithoutOptionsHasNothingToFind) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    EXPECT_FALSE(session.active());
    EXPECT_FALSE(session.searching());
    EXPECT_FALSE(session.error().has_value());
    EXPECT_EQ(session.count(), 0);
    EXPECT_EQ(session.position(), 0);
    EXPECT_FALSE(session.find_next().found);
    EXPECT_FALSE(session.find_previous().found);
}

TEST_F(SearchSessionTest, TheScanCountsTheMatchesInTheBuffer) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    EXPECT_TRUE(session.active());
    EXPECT_TRUE(session.searching());
    wait_for_count(session, 3);
    EXPECT_EQ(session.count(), 3);
    EXPECT_FALSE(session.searching());
}

TEST_F(SearchSessionTest, MatchCaseAndWholeWordChangeTheCount) {
    set_text("Abc abc ABC abcd");
    SearchSession session(buffer_.get());
    session.set_options(plain("abc"));
    wait_for_count(session, 4);
    EXPECT_EQ(session.count(), 4);
    session.set_options(plain("abc", true));
    wait_for_count(session, 2);
    EXPECT_EQ(session.count(), 2);
    session.set_options(plain("abc", false, true));
    wait_for_count(session, 3);
    EXPECT_EQ(session.count(), 3);
    session.set_options(plain("abc", true, true));
    wait_for_count(session, 1);
    EXPECT_EQ(session.count(), 1);
}

TEST_F(SearchSessionTest, RegularExpressionIsOnlyUsedWhenTheOptionIsOn) {
    set_text("a1 b22 c333");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("\\d+"));
    wait_for_count(session, 3);
    EXPECT_EQ(session.count(), 3);
    session.set_options(plain("\\d+"));
    wait_for_count(session, 0);
    EXPECT_EQ(session.count(), 0);
}

TEST_F(SearchSessionTest, WholeWordKeepsAnAlternationInsideTheWordBoundaries) {
    set_text("foobar foo");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("foo|bar", true));
    wait_for_count(session, 1);
    EXPECT_EQ(session.count(), 1);
    FindResult result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_EQ(selection_start(), 7);
    EXPECT_EQ(selection_end(), 10);
}

TEST_F(SearchSessionTest, AnInvalidPatternSearchesNothingAndSaysWhy) {
    set_text("a(b");
    SearchSession session(buffer_.get());
    session.set_options(regex_of("("));
    EXPECT_TRUE(session.error().has_value());
    EXPECT_FALSE(session.active());
    EXPECT_FALSE(session.searching());
    EXPECT_EQ(session.count(), 0);
    EXPECT_FALSE(session.find_next().found);
    session.set_options(plain("("));
    EXPECT_FALSE(session.error().has_value());
    EXPECT_TRUE(session.active());
    wait_for_count(session, 1);
    EXPECT_EQ(session.count(), 1);
    EXPECT_TRUE(session.find_next().found);
}

TEST_F(SearchSessionTest, AnEmptyTextSwitchesTheSearchOff) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    wait_for_count(session, 2);
    EXPECT_EQ(session.count(), 2);
    session.set_options(plain(""));
    EXPECT_FALSE(session.active());
    EXPECT_FALSE(session.searching());
    EXPECT_EQ(session.count(), 0);
    EXPECT_FALSE(session.find_next().found);
}

TEST_F(SearchSessionTest, FindNextVisitsTheMatchesInOrderAndWrapsAtTheEnd) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    FindResult result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 0);
    EXPECT_EQ(selection_end(), 2);
    EXPECT_EQ(cursor(), 2);
    result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 5);
    result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 6);
    EXPECT_EQ(selection_end(), 8);
    result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(selection_start(), 0);
    EXPECT_EQ(selection_end(), 2);
}

TEST_F(SearchSessionTest, FindPreviousVisitsTheMatchesBackwardsAndWrapsAtTheStart) {
    set_text("ab ab ab");
    select(8, 8);
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    FindResult result = session.find_previous();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 6);
    EXPECT_EQ(selection_end(), 8);
    EXPECT_EQ(cursor(), 6);
    result = session.find_previous();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 3);
    result = session.find_previous();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 0);
    result = session.find_previous();
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(selection_start(), 6);
    EXPECT_EQ(selection_end(), 8);
}

TEST_F(SearchSessionTest, TheSearchStartsFromTheCurrentSelection) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    select(3, 5);
    EXPECT_TRUE(session.find_next().found);
    EXPECT_EQ(selection_start(), 6);
    select(3, 5);
    EXPECT_TRUE(session.find_previous().found);
    EXPECT_EQ(selection_start(), 0);
}

TEST_F(SearchSessionTest, NothingFoundLeavesTheSelectionAlone) {
    set_text("ab ab");
    select(1, 2);
    SearchSession session(buffer_.get());
    session.set_options(plain("zz"));
    FindResult result = session.find_next();
    EXPECT_FALSE(result.found);
    EXPECT_FALSE(result.wrapped);
    result = session.find_previous();
    EXPECT_FALSE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(selection_start(), 1);
    EXPECT_EQ(selection_end(), 2);
}

TEST_F(SearchSessionTest, TheOnlyMatchIsFoundAgainAsWrapped) {
    set_text("xx ab xx");
    select(3, 5);
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    FindResult result = session.find_next();
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(selection_start(), 3);
    result = session.find_previous();
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 5);
}

TEST_F(SearchSessionTest, ThePositionNamesTheSelectedMatch) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    wait_for_count(session, 3);
    select(3, 5);
    EXPECT_EQ(session.position(), 2);
    select(0, 2);
    EXPECT_EQ(session.position(), 1);
    select(0, 0);
    EXPECT_EQ(session.position(), 0);
    select(4, 5);
    EXPECT_EQ(session.position(), 0);
}

TEST_F(SearchSessionTest, TheCountFollowsEditsOfTheBuffer) {
    set_text("ab ab");
    SearchSession session(buffer_.get());
    session.set_options(plain("ab"));
    wait_for_count(session, 2);
    EXPECT_EQ(session.count(), 2);
    gtk_text_buffer_insert_at_cursor(text_buffer(), "ab ", -1);
    wait_for_count(session, 3);
    EXPECT_EQ(session.count(), 3);
}

TEST_F(SearchSessionTest, TheCallbackHearsAboutTheCount) {
    set_text("ab ab ab");
    SearchSession session(buffer_.get());
    int calls = 0;
    session.set_on_count_changed([&calls] { ++calls; });
    session.set_options(plain("ab"));
    wait_for_count(session, 3);
    EXPECT_GT(calls, 0);
}

TEST_F(SearchSessionTest, ADestroyedSessionLeavesTheBufferAlone) {
    set_text("ab ab ab");
    int calls = 0;
    {
        SearchSession session(buffer_.get());
        session.set_on_count_changed([&calls] { ++calls; });
        session.set_options(plain("ab"));
        wait_for_count(session, 3);
    }
    const int before = calls;
    gtk_text_buffer_insert_at_cursor(text_buffer(), "ab ", -1);
    for (int i = 0; i < 100; ++i) {
        g_main_context_iteration(nullptr, FALSE);
    }
    EXPECT_EQ(calls, before);
}

} // namespace
