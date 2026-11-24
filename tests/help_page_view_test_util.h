// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Helpers shared by the tests of HelpPageView: a window with one view, and the look-ups on it.

#pragma once

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_text.h"
#include "ui/help_page_view.h"

namespace vermilion::ui::help_test {

inline bool g_display = false;

// Runs the main loop for about `ms` milliseconds.
inline void spin(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

inline ManualText build(const std::string& markdown) {
    return vermilion::build_manual_text(vermilion::parse_manual(markdown),
                                        vermilion::kManualColumns);
}

// Plain text with no ranges.
inline ManualText bare(const std::string& text) {
    ManualText page;
    page.text = text;
    page.length = vermilion::character_count(text);
    return page;
}

// A window of the given size that holds one view. `show_window` false leaves it unmapped.
struct Shown {
    GtkWidget* window = nullptr;
    std::vector<std::string> links;
    std::unique_ptr<HelpPageView> view;

    Shown(int width, int height, bool present = true) {
        window = gtk_window_new();
        gtk_window_set_default_size(GTK_WINDOW(window), width, height);
        view = std::make_unique<HelpPageView>(
            [this](const std::string& target) { links.push_back(target); });
        gtk_window_set_child(GTK_WINDOW(window), view->widget());
        if (present) {
            gtk_window_present(GTK_WINDOW(window));
            spin(300);
        }
    }
    ~Shown() {
        view.reset();
        gtk_window_destroy(GTK_WINDOW(window));
    }
    GtkTextView* text() const { return view->text_view(); }
    GtkAdjustment* vadjustment() const { return view->vadjustment(); }
    GtkAdjustment* hadjustment() const { return view->hadjustment(); }
    // The characters on the first display line of the page.
    int first_line_chars() const {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(text());
        GtkTextIter at;
        gtk_text_buffer_get_start_iter(buffer, &at);
        gtk_text_view_forward_display_line(text(), &at);
        return gtk_text_iter_get_offset(&at);
    }
    // The x of the character at `offset` in the buffer, and its y in widget coordinates.
    GdkRectangle where(int offset) const {
        GtkTextIter at;
        gtk_text_buffer_get_iter_at_offset(gtk_text_view_get_buffer(text()), &at, offset);
        GdkRectangle rectangle;
        gtk_text_view_get_iter_location(text(), &at, &rectangle);
        return rectangle;
    }
    bool has_tag(const char* name, int offset) const {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(text());
        GtkTextTag* tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
        GtkTextIter at;
        gtk_text_buffer_get_iter_at_offset(buffer, &at, offset);
        return tag != nullptr && gtk_text_iter_has_tag(&at, tag);
    }
    int tag_weight(const char* name) const {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(text());
        GtkTextTag* tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
        int weight = 0;
        g_object_get(tag, "weight", &weight, nullptr);
        return weight;
    }
    int tag_underline(const char* name) const {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(text());
        GtkTextTag* tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
        int underline = 0;
        g_object_get(tag, "underline", &underline, nullptr);
        return underline;
    }
    double tag_scale(const char* name) const {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(text());
        GtkTextTag* tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), name);
        double scale = 0.0;
        g_object_get(tag, "scale", &scale, nullptr);
        return scale;
    }
    // The controller with the given name on `widget` (names are unique there).
    static gpointer controller(GtkWidget* widget, const char* name) {
        GListModel* all = gtk_widget_observe_controllers(widget);
        gpointer found = nullptr;
        for (guint i = 0; i < g_list_model_get_n_items(all); ++i) {
            gpointer item = g_list_model_get_item(all, i);
            const char* own = gtk_event_controller_get_name(GTK_EVENT_CONTROLLER(item));
            if (own != nullptr && std::string(own) == name) {
                found = item;
            }
            g_object_unref(item);
        }
        g_object_unref(all);
        return found;
    }
    // The outer window of the widget: the one that scrolls sideways and sits left of the bar.
    GtkWidget* outer() const { return gtk_widget_get_first_child(view->widget()); }
    // The point inside the character at `offset`, in widget coordinates.
    void centre_of(int offset, double& x, double& y) const {
        GdkRectangle rectangle = where(offset);
        int wx = 0;
        int wy = 0;
        gtk_text_view_buffer_to_window_coords(text(), GTK_TEXT_WINDOW_WIDGET,
                                              rectangle.x + rectangle.width / 2,
                                              rectangle.y + rectangle.height / 2, &wx, &wy);
        x = wx;
        y = wy;
    }
};

class HelpPageViewTest : public ::testing::Test {
protected:
    // The program's stylesheet gives the view a monospace font; here a rule of the same kind does.
    static void SetUpTestSuite() {
        g_display = gtk_init_check();
        if (g_display) {
            GtkCssProvider* provider = gtk_css_provider_new();
            gtk_css_provider_load_from_string(
                provider, "textview.vm-help-view { font-family: monospace; font-size: 12pt; }");
            gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                       GTK_STYLE_PROVIDER(provider),
                                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            g_object_unref(provider);
        }
    }
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
    }
};

inline constexpr char kMarkdown[] =
    "# Title\n\nSome **bold** and `code` and [a link](open-and-save.md) here.\n\n- item one\n\n"
    "## Second\n\n### Third\n\n#### Fourth\n\n| Head A | Head B |\n|---|---|\n| one | two |\n";

} // namespace vermilion::ui::help_test
