// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: zoom, scrolling to a heading and the mouse wheel.

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

// A page of many paragraphs with the heading "Target" in the middle; returns its index.
ManualText long_page(std::size_t& target) {
    std::string markdown;
    for (int i = 0; i < 400; ++i) {
        if (i == 200) {
            markdown += "## Target\n\n";
        }
        if (i == 50) {
            markdown += "## Early\n\n";
        }
        markdown += "Paragraph " + std::to_string(i) + " " + std::string(40, 'z') + "\n\n";
    }
    ManualText page = build(markdown);
    target = 1;
    EXPECT_EQ(page.headings.size(), std::size_t{2});
    return page;
}

// Waits until the vertical position has left zero and then stayed the same for 300 ms, or `ms`
// have passed: GTK corrects the position while it lays the page out.
void wait_for_scroll(const Shown& shown, int ms) {
    double last = -1.0;
    int steady = 0;
    for (int waited = 0; waited < ms && steady < 300; waited += 20) {
        spin(20);
        const double value = gtk_adjustment_get_value(shown.vadjustment());
        steady = (value != 0.0 && value == last) ? steady + 20 : 0;
        last = value;
    }
}

// The y of the top of heading `index`, in the coordinates of the scrolled window.
double heading_y(const Shown& shown, const ManualText& page, std::size_t index) {
    return shown.where(static_cast<int>(page.headings[index].begin)).y +
           vermilion::ui::kHelpPageMargin;
}

TEST_F(HelpPageViewTest, TheWrapStaysAtAHundredAtEveryZoomAndInANarrowWindow) {
    Shown shown(600, 500);
    shown.view->show(bare(std::string(400, 'x')));
    const double zooms[] = {0.5, 0.85, 1.0, 1.3, 2.0, 3.0};
    for (double zoom : zooms) {
        shown.view->set_zoom(zoom);
        spin(300);
        EXPECT_EQ(shown.first_line_chars(), 100) << "zoom " << zoom;
        EXPECT_EQ(gtk_widget_get_width(GTK_WIDGET(shown.text())), shown.view->page_width())
            << "zoom " << zoom;
    }
}

TEST_F(HelpPageViewTest, ZoomMovesThroughItsSteps) {
    Shown shown(900, 500);
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.0);
    EXPECT_TRUE(shown.view->zoom_in());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.15);
    EXPECT_TRUE(shown.view->zoom_in());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.3);
    EXPECT_TRUE(shown.view->zoom_out());
    EXPECT_TRUE(shown.view->zoom_out());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.0);
    EXPECT_TRUE(shown.view->zoom_out());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 0.85);
    EXPECT_TRUE(shown.view->reset_zoom());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.0);
    EXPECT_FALSE(shown.view->reset_zoom());
}

TEST_F(HelpPageViewTest, ZoomStopsAtItsLimits) {
    Shown shown(900, 500);
    EXPECT_TRUE(shown.view->set_zoom(10.0));
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 3.0);
    EXPECT_FALSE(shown.view->zoom_in());
    EXPECT_FALSE(shown.view->set_zoom(3.0));
    EXPECT_TRUE(shown.view->set_zoom(0.1));
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 0.5);
    EXPECT_FALSE(shown.view->zoom_out());
    shown.view->set_zoom(1.2);
    EXPECT_TRUE(shown.view->zoom_in());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.3);
    shown.view->set_zoom(1.2);
    EXPECT_TRUE(shown.view->zoom_out());
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.15);
}

TEST_F(HelpPageViewTest, ZoomScalesThePageWidth) {
    Shown shown(900, 500);
    const int base = shown.view->page_width();
    shown.view->set_zoom(2.0);
    const double ratio = static_cast<double>(shown.view->page_width()) / base;
    EXPECT_GT(ratio, 1.9);
    EXPECT_LT(ratio, 2.1);
    EXPECT_NEAR(shown.tag_scale(vermilion::ui::kTagBody), 2.0, 1e-9);
    EXPECT_NEAR(shown.tag_scale(vermilion::ui::kTagHeading1), 3.0, 1e-9);
    EXPECT_NEAR(shown.tag_scale(vermilion::ui::kTagHeading2), 2.5, 1e-9);
    EXPECT_NEAR(shown.tag_scale(vermilion::ui::kTagHeading3), 2.0, 1e-9);
}

TEST_F(HelpPageViewTest, ZoomActionsAreOnTheWidget) {
    Shown shown(900, 500);
    EXPECT_TRUE(gtk_widget_activate_action(shown.view->widget(), "page.zoom-in", nullptr));
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.15);
    EXPECT_TRUE(gtk_widget_activate_action(shown.view->widget(), "page.zoom-out", nullptr));
    EXPECT_TRUE(gtk_widget_activate_action(shown.view->widget(), "page.zoom-out", nullptr));
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 0.85);
    EXPECT_TRUE(gtk_widget_activate_action(shown.view->widget(), "page.zoom-reset", nullptr));
    EXPECT_DOUBLE_EQ(shown.view->zoom(), 1.0);
}

TEST_F(HelpPageViewTest, TheZoomShortcutsAreTheCtrlKeysOfTheWidget) {
    Shown shown(900, 500);
    GListModel* controllers = gtk_widget_observe_controllers(shown.view->widget());
    GtkShortcutController* keys = nullptr;
    for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
        gpointer item = g_list_model_get_item(controllers, i);
        if (GTK_IS_SHORTCUT_CONTROLLER(item)) {
            keys = GTK_SHORTCUT_CONTROLLER(item);
        }
        g_object_unref(item);
    }
    g_object_unref(controllers);
    ASSERT_NE(keys, nullptr);
    std::string found;
    GListModel* shortcuts = G_LIST_MODEL(keys);
    for (guint i = 0; i < g_list_model_get_n_items(shortcuts); ++i) {
        GtkShortcut* shortcut = GTK_SHORTCUT(g_list_model_get_item(shortcuts, i));
        GtkShortcutTrigger* trigger = gtk_shortcut_get_trigger(shortcut);
        GtkShortcutAction* action = gtk_shortcut_get_action(shortcut);
        ASSERT_TRUE(GTK_IS_KEYVAL_TRIGGER(trigger));
        ASSERT_TRUE(GTK_IS_NAMED_ACTION(action));
        EXPECT_EQ(gtk_keyval_trigger_get_modifiers(GTK_KEYVAL_TRIGGER(trigger)), GDK_CONTROL_MASK);
        found += std::to_string(gtk_keyval_trigger_get_keyval(GTK_KEYVAL_TRIGGER(trigger))) + ">" +
                 gtk_named_action_get_action_name(GTK_NAMED_ACTION(action)) + " ";
        g_object_unref(shortcut);
    }
    const auto pair = [](guint key, const char* action) {
        return std::to_string(key) + ">" + action + " ";
    };
    EXPECT_EQ(found,
              pair(GDK_KEY_equal, "page.zoom-in") + pair(GDK_KEY_plus, "page.zoom-in") +
                  pair(GDK_KEY_KP_Add, "page.zoom-in") + pair(GDK_KEY_minus, "page.zoom-out") +
                  pair(GDK_KEY_KP_Subtract, "page.zoom-out") + pair(GDK_KEY_0, "page.zoom-reset") +
                  pair(GDK_KEY_KP_0, "page.zoom-reset"));
}

TEST_F(HelpPageViewTest, ScrollToHeadingPutsItAtTheTop) {
    Shown shown(900, 500);
    std::size_t target = 0;
    ManualText page = long_page(target);
    shown.view->show(page);
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
    EXPECT_TRUE(shown.view->scroll_to_heading(target));
    wait_for_scroll(shown, 3000);
    const double value = gtk_adjustment_get_value(shown.vadjustment());
    EXPECT_GT(value, 1000.0);
    EXPECT_NEAR(value, heading_y(shown, page, target), 1.0);
}

TEST_F(HelpPageViewTest, ALaterScrollReplacesAnEarlierOne) {
    Shown shown(900, 500);
    std::size_t target = 0;
    ManualText page = long_page(target);
    shown.view->show(page);
    EXPECT_TRUE(shown.view->scroll_to_heading(target));
    EXPECT_TRUE(shown.view->scroll_to_heading(0));
    wait_for_scroll(shown, 3000);
    const double value = gtk_adjustment_get_value(shown.vadjustment());
    EXPECT_GT(value, 100.0);
    EXPECT_NEAR(value, heading_y(shown, page, 0), 1.0);
}

TEST_F(HelpPageViewTest, ScrollToAMissingHeadingDoesNothing) {
    Shown shown(900, 500);
    std::size_t target = 0;
    ManualText page = long_page(target);
    shown.view->show(page);
    EXPECT_FALSE(shown.view->scroll_to_heading(2));
    EXPECT_FALSE(shown.view->scroll_to_heading(99));
    spin(500);
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
}

TEST_F(HelpPageViewTest, ShowCancelsAQueuedScrollAndGoesToTheTop) {
    Shown shown(900, 500);
    std::size_t target = 0;
    ManualText page = long_page(target);
    shown.view->show(page);
    EXPECT_TRUE(shown.view->scroll_to_heading(target));
    shown.view->show(page);
    spin(800);
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
    EXPECT_TRUE(shown.view->scroll_to_heading(target));
    wait_for_scroll(shown, 3000);
    EXPECT_GT(gtk_adjustment_get_value(shown.vadjustment()), 1000.0);
    shown.view->show(page);
    EXPECT_DOUBLE_EQ(gtk_adjustment_get_value(shown.vadjustment()), 0.0);
}

TEST_F(HelpPageViewTest, AScrollAskedBeforeTheWindowIsShownRunsWhenItIsShown) {
    Shown shown(900, 500, false);
    std::size_t target = 0;
    ManualText page = long_page(target);
    shown.view->show(page);
    EXPECT_TRUE(shown.view->scroll_to_heading(target));
    spin(300);
    gtk_window_present(GTK_WINDOW(shown.window));
    wait_for_scroll(shown, 3000);
    EXPECT_GT(gtk_adjustment_get_value(shown.vadjustment()), 1000.0);
    EXPECT_NEAR(gtk_adjustment_get_value(shown.vadjustment()), heading_y(shown, page, target), 1.0);
}

TEST_F(HelpPageViewTest, AWheelTurnBesideThePageScrollsIt) {
    Shown shown(1900, 500);
    std::size_t target = 0;
    shown.view->show(long_page(target));
    spin(300);
    GObject* wheel = static_cast<GObject*>(
        Shown::controller(shown.outer(), vermilion::ui::kHelpScrollController));
    ASSERT_NE(wheel, nullptr);
    gboolean handled = FALSE;
    g_signal_emit_by_name(wheel, "scroll", 0.0, 3.0, &handled);
    EXPECT_TRUE(handled);
    const double down = gtk_adjustment_get_value(shown.vadjustment());
    EXPECT_GT(down, 50.0);
    g_signal_emit_by_name(wheel, "scroll", 0.0, -1.0, &handled);
    EXPECT_LT(gtk_adjustment_get_value(shown.vadjustment()), down);
}

TEST_F(HelpPageViewTest, DestroyingTheViewEndsAScrollThatIsStillTrying) {
    {
        Shown shown(900, 500);
        std::size_t target = 0;
        shown.view->show(long_page(target));
        EXPECT_TRUE(shown.view->scroll_to_heading(target));
    }
    spin(400);
    SUCCEED();
}

} // namespace
