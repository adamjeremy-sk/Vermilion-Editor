// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: the width of the page, its layout for that width, and
// the new layout, with the reading position kept, after a resize or a zoom.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "core/manual_page.h"
#include "core/manual_table.h"
#include "core/manual_text.h"
#include "help_page_view_test_util.h"
#include "ui/help_page_view.h"

namespace {

using namespace vermilion::ui::help_test;
using vermilion::ManualPage;

// Paragraphs, a heading "Before", a table with three wide columns, a heading "After" and more
// paragraphs. The longest word of the table is 24 characters long.
ManualPage table_page() {
    std::string markdown = "# Top\n\n";
    for (int i = 0; i < 60; ++i) {
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    markdown += "## Before\n\n| Name | Meaning | Notes |\n|---|---|---|\n";
    for (int i = 0; i < 4; ++i) {
        markdown +=
            "| first second third fourth fifth | alpha beta gamma delta epsilon zeta theta iota "
            "kappa | twenty_four_chars_long__ one two three four five six seven eight nine ten |\n";
    }
    markdown += "\n## After\n\n";
    for (int i = 0; i < 200; ++i) {
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'y') + "\n\n";
    }
    return vermilion::parse_manual(markdown);
}

// Makes the window `width` by `height` and waits for the layout that follows.
void resize(const Shown& shown, int width, int height) {
    gtk_window_set_default_size(GTK_WINDOW(shown.window), width, height);
    spin(500);
}

int width_of_text(const Shown& shown) {
    return gtk_widget_get_width(GTK_WIDGET(shown.text()));
}

// The width of one character of body text on the screen, in pixels (a page of table_page() starts
// with a heading, which is bigger: this takes two characters of the first paragraph).
double advance_of(const Shown& shown) {
    const std::size_t found = shown.view->page().text.find("Paragraph");
    const int at = found == std::string::npos ? 0 : static_cast<int>(found);
    return shown.where(at + 1).x - shown.where(at).x;
}

TEST_F(HelpPageViewTest, ThePageWrapsToTheWidthOfTheWindow) {
    Shown shown(900, 500);
    shown.view->show(bare(std::string(500, 'x')));
    spin(300);
    EXPECT_EQ(width_of_text(shown), gtk_widget_get_width(shown.outer()));
    const int wide = shown.first_line_chars();
    EXPECT_NEAR(wide, width_of_text(shown) / advance_of(shown), 2.5);
    resize(shown, 600, 500);
    EXPECT_EQ(width_of_text(shown), gtk_widget_get_width(shown.outer()));
    EXPECT_LT(shown.first_line_chars(), wide);
    EXPECT_NEAR(shown.first_line_chars(), width_of_text(shown) / advance_of(shown), 2.5);
}

TEST_F(HelpPageViewTest, ThePageFillsAWideWindowFromTheLeftEdge) {
    Shown shown(1900, 500);
    shown.view->show(bare("short"));
    spin(300);
    graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0);
    graphene_point_t in_scroller;
    ASSERT_TRUE(
        gtk_widget_compute_point(GTK_WIDGET(shown.text()), shown.outer(), &origin, &in_scroller));
    EXPECT_NEAR(in_scroller.x, 0.0, 0.5);
    EXPECT_EQ(width_of_text(shown), gtk_widget_get_width(shown.outer()));
}

TEST_F(HelpPageViewTest, TheTablesAreLaidOutForTheColumnsOfTheWindow) {
    Shown shown(1500, 500);
    const ManualPage page = table_page();
    shown.view->show_source(page);
    spin(300);
    const std::size_t wide = shown.view->columns();
    EXPECT_GT(wide, std::size_t{100});
    EXPECT_EQ(shown.view->page().text, vermilion::build_manual_text(page, wide).text);
    EXPECT_NEAR(static_cast<double>(wide), width_of_text(shown) / advance_of(shown) - 1.5, 1.5);
    resize(shown, 900, 500);
    const std::size_t narrow = shown.view->columns();
    EXPECT_LT(narrow, wide);
    EXPECT_GE(narrow, vermilion::page_min_columns(page));
    EXPECT_EQ(shown.view->page().text, vermilion::build_manual_text(page, narrow).text);
    resize(shown, 1500, 500);
    EXPECT_GT(shown.view->columns(), narrow);
    EXPECT_EQ(shown.view->page().text,
              vermilion::build_manual_text(page, shown.view->columns()).text);
    EXPECT_NEAR(static_cast<double>(shown.view->columns()),
                width_of_text(shown) / advance_of(shown) - 1.5, 1.5);
}

TEST_F(HelpPageViewTest, ANarrowWindowScrollsSidewaysAtTheTableMinimum) {
    Shown shown(400, 400);
    const ManualPage page = table_page();
    const std::size_t least = vermilion::page_min_columns(page);
    ASSERT_GT(least, std::size_t{30});
    shown.view->show_source(page);
    spin(300);
    EXPECT_EQ(shown.view->columns(), least);
    const double needed = (static_cast<double>(least) + 1.5) * advance_of(shown);
    EXPECT_GE(width_of_text(shown), needed - 1.0);
    EXPECT_GT(width_of_text(shown), gtk_widget_get_width(shown.outer()));
    EXPECT_GE(gtk_adjustment_get_upper(shown.hadjustment()), width_of_text(shown));
    EXPECT_LT(gtk_adjustment_get_page_size(shown.hadjustment()), width_of_text(shown));
}

TEST_F(HelpPageViewTest, APageWithoutATableNeverScrollsSideways) {
    Shown shown(300, 400);
    shown.view->show_source(vermilion::parse_manual("# Title\n\nA paragraph with words.\n"));
    spin(300);
    EXPECT_EQ(width_of_text(shown), gtk_widget_get_width(shown.outer()));
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_upper(shown.hadjustment()),
                     gtk_adjustment_get_page_size(shown.hadjustment()));
}

TEST_F(HelpPageViewTest, TheReaderKeepsTheirPlaceWhenTheWindowIsResized) {
    Shown shown(1500, 500);
    const ManualPage page = table_page();
    shown.view->show_source(page);
    spin(300);
    const std::size_t after = 1; // the heading "After"
    ASSERT_EQ(shown.view->page().headings.size(), std::size_t{3});
    ASSERT_TRUE(shown.view->scroll_to_heading(after));
    spin(1500);
    const auto top_of = [&]() {
        return shown.where(static_cast<int>(shown.view->page().headings[after].begin)).y +
               vermilion::ui::kHelpPageMargin;
    };
    ASSERT_NEAR(gtk_adjustment_get_value(shown.vadjustment()), top_of(), 1.0);
    ASSERT_GT(gtk_adjustment_get_value(shown.vadjustment()), 500.0);
    const std::size_t before_columns = shown.view->columns();
    resize(shown, 800, 500);
    spin(1500);
    ASSERT_NE(shown.view->columns(), before_columns);
    EXPECT_NEAR(gtk_adjustment_get_value(shown.vadjustment()), top_of(), 2.0);
}

TEST_F(HelpPageViewTest, ManyQuickResizesGiveOneLayoutForTheLastWidth) {
    Shown shown(1500, 500);
    const ManualPage page = table_page();
    shown.view->show_source(page);
    spin(300);
    for (const int width : {1400, 1300, 1100, 1000, 900}) {
        gtk_window_set_default_size(GTK_WINDOW(shown.window), width, 500);
        spin(20);
    }
    spin(600);
    EXPECT_EQ(shown.view->page().text,
              vermilion::build_manual_text(page, shown.view->columns()).text);
    EXPECT_NEAR(static_cast<double>(shown.view->columns()),
                width_of_text(shown) / advance_of(shown) - 1.5, 1.5);
}

TEST_F(HelpPageViewTest, ZoomLaysTheTablesOutAgainAtOnce) {
    Shown shown(1500, 500);
    const ManualPage page = table_page();
    shown.view->show_source(page);
    spin(300);
    const std::size_t at_one = shown.view->columns();
    ASSERT_TRUE(shown.view->set_zoom(2.0));
    const std::size_t at_two = shown.view->columns();
    EXPECT_LT(at_two, at_one);
    EXPECT_NEAR(static_cast<double>(at_two), at_one / 2.0, 3.0);
    EXPECT_EQ(shown.view->page().text, vermilion::build_manual_text(page, at_two).text);
    ASSERT_TRUE(shown.view->reset_zoom());
    EXPECT_EQ(shown.view->columns(), at_one);
}

TEST_F(HelpPageViewTest, AFixedTextIsNeverLaidOutAgain) {
    Shown shown(1500, 500);
    const vermilion::ManualText fixed = build(kMarkdown);
    shown.view->show(fixed);
    spin(300);
    resize(shown, 700, 500);
    shown.view->set_zoom(2.0);
    spin(300);
    EXPECT_EQ(shown.view->page().text, fixed.text);
}

} // namespace
