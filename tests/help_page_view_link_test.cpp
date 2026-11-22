// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: links, clicks and the pointer.

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

TEST_F(HelpPageViewTest, AClickOnALinkCallsTheHandler) {
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    const std::size_t link = page.text.find("a link");
    EXPECT_TRUE(shown.view->activate_at(link));
    EXPECT_TRUE(shown.view->activate_at(link + 5));
    ASSERT_EQ(shown.links.size(), std::size_t{2});
    EXPECT_EQ(shown.links[0], "open-and-save.md");
    EXPECT_FALSE(shown.view->activate_at(link + 6));
    EXPECT_FALSE(shown.view->activate_at(link - 1));
    EXPECT_FALSE(shown.view->activate_at(page.length));
    EXPECT_EQ(shown.links.size(), std::size_t{2});
}

TEST_F(HelpPageViewTest, AClickOnTheViewFollowsALinkUnlessTextIsSelected) {
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    spin(300);
    GObject* click = static_cast<GObject*>(
        Shown::controller(GTK_WIDGET(shown.text()), vermilion::ui::kHelpClickController));
    ASSERT_NE(click, nullptr);
    const int link = static_cast<int>(page.text.find("a link")) + 2;
    double x = 0;
    double y = 0;
    shown.centre_of(link, x, y);
    g_signal_emit_by_name(click, "released", 1, x, y);
    ASSERT_EQ(shown.links.size(), std::size_t{1});
    EXPECT_EQ(shown.links[0], "open-and-save.md");
    shown.centre_of(2, x, y);
    g_signal_emit_by_name(click, "released", 1, x, y);
    EXPECT_EQ(shown.links.size(), std::size_t{1});
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(shown.text());
    GtkTextIter from;
    GtkTextIter to;
    gtk_text_buffer_get_iter_at_offset(buffer, &from, link - 2);
    gtk_text_buffer_get_iter_at_offset(buffer, &to, link + 3);
    gtk_text_buffer_select_range(buffer, &from, &to);
    shown.centre_of(link, x, y);
    g_signal_emit_by_name(click, "released", 1, x, y);
    EXPECT_EQ(shown.links.size(), std::size_t{1});
}

TEST_F(HelpPageViewTest, ThePointerIsAHandOverALink) {
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    spin(300);
    const auto cursor_name = [&]() {
        GdkCursor* cursor = gtk_widget_get_cursor(GTK_WIDGET(shown.text()));
        return cursor == nullptr ? std::string() : std::string(gdk_cursor_get_name(cursor));
    };
    EXPECT_EQ(cursor_name(), "text");
    const std::size_t link = page.text.find("a link");
    shown.view->hover_at(link + 1);
    EXPECT_TRUE(shown.view->pointer_on_link());
    EXPECT_EQ(cursor_name(), "pointer");
    shown.view->hover_at(link + 6);
    EXPECT_FALSE(shown.view->pointer_on_link());
    EXPECT_EQ(cursor_name(), "text");
    GObject* motion = static_cast<GObject*>(
        Shown::controller(GTK_WIDGET(shown.text()), vermilion::ui::kHelpMotionController));
    ASSERT_NE(motion, nullptr);
    double x = 0;
    double y = 0;
    shown.centre_of(static_cast<int>(link) + 2, x, y);
    g_signal_emit_by_name(motion, "motion", x, y);
    EXPECT_TRUE(shown.view->pointer_on_link());
    EXPECT_EQ(cursor_name(), "pointer");
    g_signal_emit_by_name(motion, "leave");
    EXPECT_FALSE(shown.view->pointer_on_link());
    EXPECT_EQ(cursor_name(), "text");
}

TEST_F(HelpPageViewTest, ALinkOfAReplacedPageIsGone) {
    Shown shown(900, 500);
    ManualText first = build(kMarkdown);
    shown.view->show(first);
    ManualText second = build("Only a [link](other.md).\n");
    shown.view->show(second);
    EXPECT_TRUE(shown.view->activate_at(second.text.find("link")));
    ASSERT_EQ(shown.links.size(), std::size_t{1});
    EXPECT_EQ(shown.links[0], "other.md");
    EXPECT_FALSE(shown.view->activate_at(first.text.find("a link")));
}

} // namespace
