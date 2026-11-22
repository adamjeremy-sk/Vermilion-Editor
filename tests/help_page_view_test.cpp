// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: width, wrapping, tags, links and the pointer.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstddef>
#include <string>

#include "core/manual_text.h"
#include "help_page_view_test_util.h"
#include "ui/help_page_view.h"

namespace {

using namespace vermilion::ui::help_test;
using vermilion::ManualText;

TEST_F(HelpPageViewTest, ShowPutsTheTextIntoTheView) {
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(shown.text());
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
    EXPECT_EQ(std::string(text), page.text);
    g_free(text);
    EXPECT_EQ(shown.view->page().text, page.text);
    EXPECT_FALSE(gtk_text_view_get_editable(shown.text()));
    EXPECT_EQ(gtk_text_view_get_wrap_mode(shown.text()), GTK_WRAP_CHAR);
}

TEST_F(HelpPageViewTest, ALongLineWrapsAfterExactlyAHundredColumns) {
    Shown shown(1700, 500);
    shown.view->show(bare(std::string(300, 'x')));
    spin(300);
    EXPECT_EQ(shown.first_line_chars(), 100);
}

TEST_F(HelpPageViewTest, ThePageIsCentredInAWideWindow) {
    Shown shown(1900, 500);
    shown.view->show(bare("short"));
    spin(300);
    graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0);
    graphene_point_t in_scroller;
    ASSERT_TRUE(
        gtk_widget_compute_point(GTK_WIDGET(shown.text()), shown.outer(), &origin, &in_scroller));
    const int outer = gtk_widget_get_width(shown.outer());
    const int page = shown.view->page_width();
    EXPECT_LT(page, outer);
    EXPECT_NEAR(in_scroller.x, (outer - page) / 2.0, 1.5);
}

TEST_F(HelpPageViewTest, ANarrowWindowScrollsSidewaysAndKeepsThePageWidth) {
    Shown shown(500, 400);
    shown.view->show(bare(std::string(250, 'x')));
    spin(300);
    const int page = shown.view->page_width();
    EXPECT_GT(page, 500);
    EXPECT_EQ(gtk_widget_get_width(GTK_WIDGET(shown.text())), page);
    EXPECT_GE(gtk_adjustment_get_upper(shown.hadjustment()), page);
    EXPECT_LT(gtk_adjustment_get_page_size(shown.hadjustment()), page);
    EXPECT_EQ(shown.first_line_chars(), 100);
}

TEST_F(HelpPageViewTest, TheWindowHeightHoldsTheLinesAsked) {
    Shown shown(900, 500);
    const int one = shown.view->line_height();
    EXPECT_GT(one, 0);
    EXPECT_EQ(shown.view->height_for_lines(0), 2 * vermilion::ui::kHelpPageMargin);
    EXPECT_EQ(shown.view->height_for_lines(36), 36 * one + 2 * vermilion::ui::kHelpPageMargin);
}

TEST_F(HelpPageViewTest, TagsCoverTheirRanges) {
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    const auto at = [&](const char* word) { return static_cast<int>(page.text.find(word)); };
    using namespace vermilion::ui;
    EXPECT_NEAR(shown.tag_scale(kTagBody), 1.0, 1e-9);
    EXPECT_NEAR(shown.tag_scale(kTagHeading1), 1.5, 1e-9);
    EXPECT_NEAR(shown.tag_scale(kTagHeading2), 1.25, 1e-9);
    EXPECT_NEAR(shown.tag_scale(kTagHeading3), 1.0, 1e-9);
    EXPECT_TRUE(shown.has_tag(kTagBody, 0));
    EXPECT_TRUE(shown.has_tag(kTagBody, static_cast<int>(page.length) - 1));
    EXPECT_TRUE(shown.has_tag(kTagHeading1, at("Title")));
    EXPECT_FALSE(shown.has_tag(kTagHeading1, at("Some")));
    EXPECT_EQ(shown.tag_weight(kTagHeading1), PANGO_WEIGHT_BOLD);
    EXPECT_EQ(shown.tag_weight(kTagHeading2), PANGO_WEIGHT_BOLD);
    EXPECT_EQ(shown.tag_weight(kTagHeading3), PANGO_WEIGHT_BOLD);
    EXPECT_TRUE(shown.has_tag(kTagHeading2, at("Second")));
    EXPECT_TRUE(shown.has_tag(kTagHeading3, at("Third")));
    EXPECT_TRUE(shown.has_tag(kTagHeading3, at("Fourth")));
    EXPECT_FALSE(shown.has_tag(kTagHeading1, at("Second")));
    EXPECT_EQ(shown.tag_weight(kTagBold), PANGO_WEIGHT_BOLD);
    EXPECT_EQ(shown.tag_weight(kTagTableHeader), PANGO_WEIGHT_BOLD);
    EXPECT_EQ(shown.tag_underline(kTagLink), PANGO_UNDERLINE_SINGLE);
    EXPECT_TRUE(shown.has_tag(kTagBold, at("bold")));
    EXPECT_TRUE(shown.has_tag(kTagBold, at("bold") + 3));
    EXPECT_FALSE(shown.has_tag(kTagBold, at("bold") + 4));
    EXPECT_FALSE(shown.has_tag(kTagBold, at("Some")));
    EXPECT_TRUE(shown.has_tag(kTagCode, at("code")));
    EXPECT_FALSE(shown.has_tag(kTagCode, at("and")));
    EXPECT_TRUE(shown.has_tag(kTagLink, at("a link")));
    EXPECT_TRUE(shown.has_tag(kTagLink, at("a link") + 5));
    EXPECT_FALSE(shown.has_tag(kTagLink, at("a link") + 6));
    EXPECT_TRUE(shown.has_tag(kTagItem, at("item one")));
    EXPECT_TRUE(shown.has_tag(kTagItem, at("\xE2\x80\xA2")));
    EXPECT_FALSE(shown.has_tag(kTagItem, at("Some")));
    EXPECT_TRUE(shown.has_tag(kTagTableHeader, at("Head A")));
    EXPECT_FALSE(shown.has_tag(kTagTableHeader, at("one |")));
}

TEST_F(HelpPageViewTest, AListItemHangsByTwoColumns) {
    Shown shown(1700, 500);
    ManualText page = build("- " + std::string(250, 'y') + "\n");
    ASSERT_EQ(page.list_items.size(), std::size_t{1});
    shown.view->show(page);
    spin(300);
    EXPECT_EQ(shown.first_line_chars(), 100);
    const GdkRectangle first = shown.where(0);
    const GdkRectangle second_line = shown.where(100);
    const double advance = shown.where(3).x - shown.where(2).x;
    EXPECT_EQ(first.x, 0);
    EXPECT_GT(second_line.y, first.y);
    EXPECT_NEAR(second_line.x, 2 * advance, 1.0);
}

TEST_F(HelpPageViewTest, ShowReplacesThePage) {
    Shown shown(900, 500);
    ManualText first = build(kMarkdown);
    shown.view->show(first);
    ManualText second = build("Only a [link](other.md).\n");
    shown.view->show(second);
    EXPECT_EQ(shown.view->page().text, second.text);
    EXPECT_FALSE(shown.has_tag(vermilion::ui::kTagBold, 0));
}

TEST_F(HelpPageViewTest, BadUtf8NeverWarnsOrBreaksTheView) {
    Shown shown(900, 500);
    shown.view->show(bare("abc\xFF"
                          "def"));
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(shown.text());
    EXPECT_GE(gtk_text_buffer_get_char_count(buffer), 7);
}

TEST_F(HelpPageViewTest, ShowScrollsToTheTop) {
    Shown shown(900, 500);
    std::string lines;
    for (int i = 0; i < 300; ++i) {
        lines += "line " + std::to_string(i) + "\n";
    }
    shown.view->show(bare(lines));
    spin(300);
    gtk_adjustment_set_value(shown.vadjustment(), 400.0);
    EXPECT_GT(gtk_adjustment_get_value(shown.vadjustment()), 300.0);
    shown.view->show(bare(lines));
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
}

TEST_F(HelpPageViewTest, TheWidgetIsTwoScrolledWindowsAndABar) {
    Shown shown(900, 500);
    GtkWidget* outer = shown.outer();
    ASSERT_TRUE(GTK_IS_SCROLLED_WINDOW(outer));
    GtkPolicyType horizontal = GTK_POLICY_NEVER;
    GtkPolicyType vertical = GTK_POLICY_ALWAYS;
    gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(outer), &horizontal, &vertical);
    EXPECT_EQ(horizontal, GTK_POLICY_AUTOMATIC);
    EXPECT_EQ(vertical, GTK_POLICY_NEVER);
    GtkWidget* inner = gtk_widget_get_ancestor(GTK_WIDGET(shown.text()), GTK_TYPE_SCROLLED_WINDOW);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner, outer);
    gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(inner), &horizontal, &vertical);
    EXPECT_EQ(horizontal, GTK_POLICY_NEVER);
    EXPECT_EQ(vertical, GTK_POLICY_EXTERNAL);
    GtkWidget* bar = gtk_widget_get_last_child(shown.view->widget());
    ASSERT_TRUE(GTK_IS_SCROLLBAR(bar));
    EXPECT_EQ(gtk_scrollbar_get_adjustment(GTK_SCROLLBAR(bar)), shown.vadjustment());
    EXPECT_EQ(gtk_orientable_get_orientation(GTK_ORIENTABLE(bar)), GTK_ORIENTATION_VERTICAL);
}

} // namespace
