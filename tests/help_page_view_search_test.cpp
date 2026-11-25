// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: the search highlights, the current match, the steps
// from match to match and the scroll to a match.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

#include "core/manual_page.h"
#include "core/manual_search.h"
#include "core/manual_text.h"
#include "core/search_options.h"
#include "help_page_view_test_util.h"
#include "ui/help_page_view.h"

namespace {

using namespace vermilion::ui::help_test;
using vermilion::ManualPage;
using vermilion::ManualText;
using vermilion::MatchStep;
using vermilion::SearchMatch;
using vermilion::SearchOptions;
using vermilion::ui::kTagCurrentMatch;
using vermilion::ui::kTagMatch;

SearchOptions options(const std::string& text, bool match_case = false, bool whole_word = false,
                      bool regex = false) {
    SearchOptions result;
    result.text = text;
    result.match_case = match_case;
    result.whole_word = whole_word;
    result.regex = regex;
    return result;
}

// Every character of the page on the screen has kTagMatch exactly when a match of the view covers
// it, and kTagCurrentMatch exactly when match number `current` covers it (no character has it when
// `current` is empty).
void expect_highlights(const Shown& shown, std::optional<std::size_t> current) {
    const auto& matches = shown.view->matches();
    ASSERT_EQ(shown.view->current_match(), current);
    for (std::size_t at = 0; at < shown.view->page().length; ++at) {
        bool in_match = false;
        for (const SearchMatch& match : matches) {
            in_match = in_match || (at >= match.begin && at < match.end);
        }
        const bool in_current =
            current.has_value() && at >= matches[*current].begin && at < matches[*current].end;
        EXPECT_EQ(shown.has_tag(kTagMatch, static_cast<int>(at)), in_match) << "match at " << at;
        EXPECT_EQ(shown.has_tag(kTagCurrentMatch, static_cast<int>(at)), in_current)
            << "current at " << at;
    }
}

// 300 lines "line n", the word "needle" on line `needle_line` (counted from 1).
ManualText long_page(int needle_line) {
    std::string text;
    for (int i = 1; i <= 300; ++i) {
        text += i == needle_line ? "needle\n" : "line " + std::to_string(i) + "\n";
    }
    return bare(text);
}

// The top of the character at `offset` in the coordinates of the vertical adjustment.
double top_of(const Shown& shown, int offset) {
    return shown.where(offset).y + vermilion::ui::kHelpPageMargin;
}

TEST_F(HelpPageViewTest, NothingIsHighlightedBeforeASearch) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    EXPECT_TRUE(shown.view->search().text.empty());
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, EveryMatchOfTheSearchIsHighlighted) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha gamma alpha"));
    EXPECT_EQ(shown.view->set_search(options("alpha")), std::size_t{3});
    EXPECT_EQ(shown.view->search(), options("alpha"));
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    EXPECT_EQ(shown.view->matches()[0], (SearchMatch{0, 5}));
    EXPECT_EQ(shown.view->matches()[1], (SearchMatch{11, 16}));
    EXPECT_EQ(shown.view->matches()[2], (SearchMatch{23, 28}));
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, TheOptionsOfTheSearchApply) {
    Shown shown(900, 500);
    shown.view->show(bare("Foo foo foobar FOO fo oo"));
    EXPECT_EQ(shown.view->set_search(options("foo")), std::size_t{4});
    EXPECT_EQ(shown.view->set_search(options("foo", true)), std::size_t{2});
    EXPECT_EQ(shown.view->set_search(options("foo", false, true)), std::size_t{3});
    EXPECT_EQ(shown.view->set_search(options("fo+", false, false, true)), std::size_t{5});
    EXPECT_EQ(shown.view->set_search(options("fo+", false, false, false)), std::size_t{0});
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, ABadPatternHighlightsNothing) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha (beta)"));
    EXPECT_EQ(shown.view->set_search(options("(", false, false, true)), std::size_t{0});
    EXPECT_EQ(shown.view->search().text, "(");
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, AnEmptySearchTakesTheHighlightsOff) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    ASSERT_TRUE(shown.view->step(true).has_value());
    EXPECT_EQ(shown.view->set_search(SearchOptions{}), std::size_t{0});
    EXPECT_TRUE(shown.view->search().text.empty());
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, StepGoesToTheFirstMatchFromNowhereAndToTheLastBackward) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha gamma alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{3});
    std::optional<MatchStep> first = shown.view->step(true);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->index, std::size_t{0});
    EXPECT_FALSE(first->wrapped);
    expect_highlights(shown, std::size_t{0});
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{3});
    expect_highlights(shown, std::nullopt);
    std::optional<MatchStep> last = shown.view->step(false);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->index, std::size_t{2});
    EXPECT_FALSE(last->wrapped);
    expect_highlights(shown, std::size_t{2});
}

TEST_F(HelpPageViewTest, StepsMoveAndWrapAtTheEnds) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha gamma alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{3});
    ASSERT_TRUE(shown.view->step(true).has_value());
    std::optional<MatchStep> second = shown.view->step(true);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->index, std::size_t{1});
    EXPECT_FALSE(second->wrapped);
    expect_highlights(shown, std::size_t{1});
    std::optional<MatchStep> third = shown.view->step(true);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->index, std::size_t{2});
    std::optional<MatchStep> wrapped = shown.view->step(true);
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(wrapped->index, std::size_t{0});
    EXPECT_TRUE(wrapped->wrapped);
    expect_highlights(shown, std::size_t{0});
    std::optional<MatchStep> back = shown.view->step(false);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->index, std::size_t{2});
    EXPECT_TRUE(back->wrapped);
    expect_highlights(shown, std::size_t{2});
    std::optional<MatchStep> before = shown.view->step(false);
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->index, std::size_t{1});
    EXPECT_FALSE(before->wrapped);
}

TEST_F(HelpPageViewTest, StepDoesNothingWithoutMatches) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta"));
    EXPECT_FALSE(shown.view->step(true).has_value());
    ASSERT_EQ(shown.view->set_search(options("zzz")), std::size_t{0});
    EXPECT_FALSE(shown.view->step(true).has_value());
    EXPECT_FALSE(shown.view->step(false).has_value());
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, ANewSearchForgetsTheCurrentMatch) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha gamma"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    ASSERT_TRUE(shown.view->step(true).has_value());
    ASSERT_EQ(shown.view->set_search(options("a")), std::size_t{7});
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, ShowingAnotherPageSearchesItAndForgetsTheCurrentMatch) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    ASSERT_TRUE(shown.view->step(true).has_value());
    shown.view->show(bare("gamma alpha delta alpha alpha"));
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    EXPECT_EQ(shown.view->matches()[0].begin, std::size_t{6});
    expect_highlights(shown, std::nullopt);
    shown.view->show(bare("nothing here"));
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown, std::nullopt);
    EXPECT_EQ(shown.view->search(), options("alpha"));
}

TEST_F(HelpPageViewTest, ASourcePageIsSearchedToo) {
    Shown shown(900, 500);
    shown.view->set_search(options("here"));
    shown.view->show_source(vermilion::parse_manual("# Title\n\nSome words here and here.\n"));
    EXPECT_EQ(shown.view->matches().size(), std::size_t{2});
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, StepScrollsAFarMatchToTheMiddleOfTheWindow) {
    Shown shown(900, 300);
    shown.view->show(long_page(150));
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{1});
    ASSERT_TRUE(shown.view->step(true).has_value());
    spin(1500);
    const int at = static_cast<int>(shown.view->matches()[0].begin);
    GtkAdjustment* adjustment = shown.vadjustment();
    const double value = gtk_adjustment_get_value(adjustment);
    const double page = gtk_adjustment_get_page_size(adjustment);
    EXPECT_GT(value, 1000.0);
    const double middle = top_of(shown, at) + shown.where(at).height / 2.0;
    EXPECT_NEAR(middle, value + page / 2.0, 3.0);
}

TEST_F(HelpPageViewTest, StepDoesNotScrollAMatchThatShows) {
    Shown shown(900, 500);
    shown.view->show(long_page(15));
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{1});
    ASSERT_TRUE(shown.view->step(true).has_value());
    spin(600);
    const int at = static_cast<int>(shown.view->matches()[0].begin);
    ASSERT_GT(top_of(shown, at), 200.0); // far enough from the top for a centred scroll to show
    EXPECT_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
}

TEST_F(HelpPageViewTest, StepBringsAMatchAboveTheWindowBackIntoView) {
    Shown shown(900, 300);
    shown.view->show(long_page(5));
    ASSERT_EQ(shown.view->set_search(options("line 29")), std::size_t{11});
    ASSERT_TRUE(shown.view->step(false).has_value()); // the last one, far down
    spin(1500);
    const double far = gtk_adjustment_get_value(shown.vadjustment());
    ASSERT_GT(far, 500.0);
    for (std::size_t i = 0; i < 10; ++i) {
        ASSERT_TRUE(shown.view->step(false).has_value());
    }
    spin(1500);
    const int at = static_cast<int>(shown.view->matches()[0].begin);
    const double value = gtk_adjustment_get_value(shown.vadjustment());
    EXPECT_LE(value, top_of(shown, at));
    EXPECT_LE(top_of(shown, at) + shown.where(at).height,
              value + gtk_adjustment_get_page_size(shown.vadjustment()) + 1.0);
}

// Paragraphs "needle", a table with the word in a cell, and a last paragraph "needle".
ManualPage table_page() {
    std::string markdown = "needle at the start\n\n";
    for (int i = 0; i < 30; ++i) {
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    markdown += "| Name | Meaning | Notes |\n|---|---|---|\n";
    for (int i = 0; i < 3; ++i) {
        markdown +=
            "| first second third fourth fifth | alpha beta gamma delta epsilon zeta theta iota "
            "kappa | twenty_four_chars_long__ one two three four five six seven eight nine ten |\n";
    }
    markdown += "| needle in a cell | two | three |\n\nneedle at the end\n";
    return vermilion::parse_manual(markdown);
}

TEST_F(HelpPageViewTest, ANewLayoutSearchesTheNewTextAndKeepsTheCurrentMatch) {
    Shown shown(1500, 500);
    const ManualPage page = table_page();
    shown.view->show_source(page);
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{3});
    ASSERT_TRUE(shown.view->step(true).has_value());
    ASSERT_TRUE(shown.view->step(true).has_value()); // the one in the table cell
    expect_highlights(shown, std::size_t{1});
    const std::size_t wide = shown.view->columns();
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 800, 500);
    spin(1500);
    ASSERT_LT(shown.view->columns(), wide);
    EXPECT_EQ(shown.view->matches(),
              vermilion::find_matches(shown.view->page().text, options("needle")));
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    expect_highlights(shown, std::size_t{1});
    ASSERT_EQ(shown.view->page().tables.size(), std::size_t{1});
    EXPECT_GE(shown.view->matches()[1].begin, shown.view->page().tables[0].begin);
    EXPECT_LT(shown.view->matches()[1].begin, shown.view->page().tables[0].end);
}

// A phrase in a paragraph, in a table cell that a narrow layout wraps in the middle of the phrase,
// and in a last paragraph.
ManualPage phrase_page() {
    std::string markdown = "needle in a haystack\n\n";
    for (int i = 0; i < 30; ++i) {
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    markdown += "| Name | Meaning | Notes |\n|---|---|---|\n";
    for (int i = 0; i < 3; ++i) {
        markdown +=
            "| first second third fourth fifth | alpha beta gamma delta epsilon zeta theta iota "
            "kappa | twenty_four_chars_long__ one two three four five six seven eight nine ten |\n";
    }
    markdown += "| needle in a cell | two | three |\n\nneedle in a stack\n";
    return vermilion::parse_manual(markdown);
}

TEST_F(HelpPageViewTest, ANewLayoutWithAnotherNumberOfMatchesKeepsTheNearestMatch) {
    Shown shown(1500, 500);
    shown.view->show_source(phrase_page());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle in a")), std::size_t{3});
    ASSERT_TRUE(shown.view->step(false).has_value()); // the last paragraph
    expect_highlights(shown, std::size_t{2});
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 800, 500);
    spin(1500);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{2}); // the cell no longer holds the phrase
    const std::size_t last = shown.view->page().text.rfind("needle in a stack");
    ASSERT_NE(last, std::string::npos);
    EXPECT_EQ(shown.view->matches()[1].begin, last);
    expect_highlights(shown, std::size_t{1});
}

// Two paragraphs with a phrase, then a table whose last cell holds the phrase too.
ManualPage phrase_at_the_end() {
    std::string markdown = "needle in a haystack\n\nneedle in a stack\n\n";
    for (int i = 0; i < 30; ++i) {
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    markdown += "| Name | Meaning | Notes |\n|---|---|---|\n";
    for (int i = 0; i < 3; ++i) {
        markdown +=
            "| first second third fourth fifth | alpha beta gamma delta epsilon zeta theta iota "
            "kappa | twenty_four_chars_long__ one two three four five six seven eight nine ten |\n";
    }
    markdown += "| needle in a cell | two | three |\n";
    return vermilion::parse_manual(markdown);
}

TEST_F(HelpPageViewTest, ANewLayoutWithNoMatchAfterTheOldPlaceKeepsTheLastMatch) {
    Shown shown(1500, 500);
    shown.view->show_source(phrase_at_the_end());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle in a")), std::size_t{3});
    ASSERT_TRUE(shown.view->step(false).has_value()); // the one in the last cell
    expect_highlights(shown, std::size_t{2});
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 800, 500);
    spin(1500);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{2}); // the cell no longer holds the phrase
    expect_highlights(shown, std::size_t{1});
}

void count_criticals(const gchar*, GLogLevelFlags, const gchar*, gpointer data) {
    ++*static_cast<int*>(data);
}

TEST_F(HelpPageViewTest, ANewSearchEndsTheScrollToTheOldCurrentMatch) {
    Shown shown(900, 300);
    shown.view->show(long_page(150));
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{1});
    ASSERT_TRUE(shown.view->step(true).has_value()); // the scroll to it is still trying
    int criticals = 0;
    const guint handler =
        g_log_set_handler("Gtk", G_LOG_LEVEL_CRITICAL, count_criticals, &criticals);
    shown.view->set_search(options("line"));
    spin(600);
    g_log_remove_handler("Gtk", handler);
    EXPECT_EQ(criticals, 0);
}

TEST_F(HelpPageViewTest, ANewLayoutWithoutACurrentMatchHasNone) {
    Shown shown(1500, 500);
    shown.view->show_source(table_page());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{3});
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 800, 500);
    spin(1500);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    expect_highlights(shown, std::nullopt);
}

TEST_F(HelpPageViewTest, ZoomKeepsTheHighlightsAndTheCurrentMatch) {
    Shown shown(1500, 500);
    shown.view->show_source(table_page());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{3});
    ASSERT_TRUE(shown.view->step(false).has_value());
    expect_highlights(shown, std::size_t{2});
    ASSERT_TRUE(shown.view->set_zoom(2.0));
    spin(300);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    expect_highlights(shown, std::size_t{2});
}

TEST_F(HelpPageViewTest, AFixedPageKeepsItsHighlightsWhenTheWindowChanges) {
    Shown shown(1500, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    ASSERT_TRUE(shown.view->step(true).has_value());
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 700, 500);
    spin(600);
    shown.view->set_zoom(1.5);
    spin(300);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{2});
    expect_highlights(shown, std::size_t{0});
}

} // namespace
