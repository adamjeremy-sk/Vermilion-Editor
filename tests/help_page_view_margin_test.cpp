// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: the margins of the text view and a heading scrolled to
// the top of the window.

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

// A page of many paragraphs with the heading "Target" in the middle (heading 0).
ManualText long_page() {
    std::string markdown;
    for (int i = 0; i < 400; ++i) {
        if (i == 200) {
            markdown += "## Target\n\n";
        }
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    ManualText page = build(markdown);
    EXPECT_EQ(page.headings.size(), std::size_t{1});
    return page;
}

// Waits until the vertical position has left zero and then stayed the same for 300 ms, or 3 s
// have passed: GTK corrects the position while it lays the page out.
void wait_for_scroll(const Shown& shown) {
    double last = -1.0;
    int steady = 0;
    for (int waited = 0; waited < 3000 && steady < 300; waited += 20) {
        spin(20);
        const double value = gtk_adjustment_get_value(shown.vadjustment());
        steady = (value != 0.0 && value == last) ? steady + 20 : 0;
        last = value;
    }
}

TEST_F(HelpPageViewTest, TheMarginsBelongToTheTextView) {
    Shown shown(900, 500);
    GtkWidget* view = GTK_WIDGET(shown.text());
    EXPECT_EQ(gtk_widget_get_margin_top(view), 0);
    EXPECT_EQ(gtk_widget_get_margin_bottom(view), 0);
    EXPECT_EQ(gtk_widget_get_margin_start(view), 0);
    EXPECT_EQ(gtk_widget_get_margin_end(view), 0);
    EXPECT_EQ(gtk_text_view_get_top_margin(shown.text()), vermilion::ui::kHelpPageMargin);
    EXPECT_EQ(gtk_text_view_get_bottom_margin(shown.text()), vermilion::ui::kHelpPageMargin);
    EXPECT_EQ(gtk_text_view_get_left_margin(shown.text()), 0);
    EXPECT_EQ(gtk_text_view_get_right_margin(shown.text()), 0);
}

// With a margin on the widget the top eight pixels of the heading were cut off.
TEST_F(HelpPageViewTest, AHeadingScrolledToTheTopIsNotClipped) {
    Shown shown(900, 500);
    ManualText page = long_page();
    shown.view->show(page);
    EXPECT_TRUE(shown.view->scroll_to_heading(0));
    wait_for_scroll(shown);
    ASSERT_GT(gtk_adjustment_get_value(shown.vadjustment()), 1000.0);
    const GdkRectangle at = shown.where(static_cast<int>(page.headings[0].begin));
    int wx = 0;
    int wy = 0;
    gtk_text_view_buffer_to_window_coords(shown.text(), GTK_TEXT_WINDOW_WIDGET, at.x, at.y, &wx,
                                          &wy);
    EXPECT_GE(wy, -1);
    EXPECT_LE(wy, 1);
}

} // namespace
