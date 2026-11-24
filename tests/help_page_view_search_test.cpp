// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: the search highlights, the current match, the steps
// from match to match and the scroll to a match.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstddef>
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
using vermilion::SearchMatch;
using vermilion::SearchOptions;
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
// it.
void expect_highlights(const Shown& shown) {
    const auto& matches = shown.view->matches();
    for (std::size_t at = 0; at < shown.view->page().length; ++at) {
        bool in_match = false;
        for (const SearchMatch& match : matches) {
            in_match = in_match || (at >= match.begin && at < match.end);
        }
        EXPECT_EQ(shown.has_tag(kTagMatch, static_cast<int>(at)), in_match) << "match at " << at;
    }
}

TEST_F(HelpPageViewTest, NothingIsHighlightedBeforeASearch) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    EXPECT_TRUE(shown.view->search().text.empty());
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown);
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
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, TheOptionsOfTheSearchApply) {
    Shown shown(900, 500);
    shown.view->show(bare("Foo foo foobar FOO fo oo"));
    EXPECT_EQ(shown.view->set_search(options("foo")), std::size_t{4});
    EXPECT_EQ(shown.view->set_search(options("foo", true)), std::size_t{2});
    EXPECT_EQ(shown.view->set_search(options("foo", false, true)), std::size_t{3});
    EXPECT_EQ(shown.view->set_search(options("fo+", false, false, true)), std::size_t{5});
    EXPECT_EQ(shown.view->set_search(options("fo+", false, false, false)), std::size_t{0});
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, ABadPatternHighlightsNothing) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha (beta)"));
    EXPECT_EQ(shown.view->set_search(options("(", false, false, true)), std::size_t{0});
    EXPECT_EQ(shown.view->search().text, "(");
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, AnEmptySearchTakesTheHighlightsOff) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    EXPECT_EQ(shown.view->set_search(SearchOptions{}), std::size_t{0});
    EXPECT_TRUE(shown.view->search().text.empty());
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, ShowingAnotherPageSearchesIt) {
    Shown shown(900, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    shown.view->show(bare("gamma alpha delta alpha alpha"));
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    EXPECT_EQ(shown.view->matches()[0].begin, std::size_t{6});
    expect_highlights(shown);
    shown.view->show(bare("nothing here"));
    EXPECT_TRUE(shown.view->matches().empty());
    expect_highlights(shown);
    EXPECT_EQ(shown.view->search(), options("alpha"));
}

TEST_F(HelpPageViewTest, ASourcePageIsSearchedToo) {
    Shown shown(900, 500);
    shown.view->set_search(options("here"));
    shown.view->show_source(vermilion::parse_manual("# Title\n\nSome words here and here.\n"));
    EXPECT_EQ(shown.view->matches().size(), std::size_t{2});
    expect_highlights(shown);
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

TEST_F(HelpPageViewTest, ANewLayoutSearchesTheNewText) {
    Shown shown(1500, 500);
    shown.view->show_source(table_page());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{3});
    const std::size_t wide = shown.view->columns();
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 800, 500);
    spin(1500);
    ASSERT_LT(shown.view->columns(), wide);
    EXPECT_EQ(shown.view->matches(),
              vermilion::find_matches(shown.view->page().text, options("needle")));
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, ZoomKeepsTheHighlights) {
    Shown shown(1500, 500);
    shown.view->show_source(table_page());
    spin(300);
    ASSERT_EQ(shown.view->set_search(options("needle")), std::size_t{3});
    ASSERT_TRUE(shown.view->set_zoom(2.0));
    spin(300);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{3});
    expect_highlights(shown);
}

TEST_F(HelpPageViewTest, AFixedPageKeepsItsHighlightsWhenTheWindowChanges) {
    Shown shown(1500, 500);
    shown.view->show(bare("alpha beta alpha"));
    ASSERT_EQ(shown.view->set_search(options("alpha")), std::size_t{2});
    gtk_window_set_default_size(GTK_WINDOW(shown.window), 700, 500);
    spin(600);
    shown.view->set_zoom(1.5);
    spin(300);
    ASSERT_EQ(shown.view->matches().size(), std::size_t{2});
    expect_highlights(shown);
}

} // namespace
