// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the zoom, the scroll to a heading and the mouse wheel of the Help page view.

#include <gtk/gtk.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

#include "ui/gtkx/signal.h"
#include "ui/help_page_view.h"

namespace vermilion::ui {
namespace {

// The steps of zoom in and out, increasing.
constexpr double kZoomSteps[] = {0.5, 0.7, 0.85, 1.0, 1.15, 1.3, 1.5, 2.0, 2.5, 3.0};
// A scroll to a heading is tried every kScrollTickMs ms until the position has been right
// kScrollSteadyTicks tries in a row, at most kScrollMaxTicks tries; while the window is not shown
// it waits, at most kScrollMaxWaits ticks.
constexpr guint kScrollTickMs = 50;
constexpr int kScrollSteadyTicks = 3;
constexpr int kScrollMaxTicks = 60;
constexpr int kScrollMaxWaits = 200;

// Adds a Ctrl+key shortcut that activates `action` to the shortcut `controller`.
void add_shortcut(GtkEventController* controller, guint key, const char* action) {
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(gtk_keyval_trigger_new(key, GDK_CONTROL_MASK),
                         gtk_named_action_new(action)));
}

} // namespace

void HelpPageView::connect_wheel() {
    GtkEventController* wheel =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL); // floating
    gtk_event_controller_set_name(wheel, kHelpScrollController);
    gtk_widget_add_controller(outer_, wheel); // the outer window owns it
    wheel_signal_ = gtkx::connect<gboolean(GtkEventControllerScroll*, double, double)>(
        wheel, "scroll", [this](GtkEventControllerScroll*, double, double dy) -> gboolean {
            on_wheel(dy);
            return TRUE;
        });
}

void HelpPageView::build_zoom_actions() {
    GSimpleActionGroup* group = g_simple_action_group_new();
    GSimpleAction* action_in = g_simple_action_new("zoom-in", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_in));
    zoom_in_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_in, "activate", [this](GSimpleAction*, GVariant*) { zoom_in(); });
    g_object_unref(action_in); // the map holds it
    GSimpleAction* action_out = g_simple_action_new("zoom-out", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_out));
    zoom_out_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_out, "activate", [this](GSimpleAction*, GVariant*) { zoom_out(); });
    g_object_unref(action_out); // the map holds it
    GSimpleAction* action_reset = g_simple_action_new("zoom-reset", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_reset));
    zoom_reset_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_reset, "activate", [this](GSimpleAction*, GVariant*) { reset_zoom(); });
    g_object_unref(action_reset); // the map holds it
    gtk_widget_insert_action_group(root_, "page", G_ACTION_GROUP(group));
    g_object_unref(group);

    GtkEventController* keys = gtk_shortcut_controller_new(); // floating; the widget owns it
    add_shortcut(keys, GDK_KEY_equal, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_plus, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_KP_Add, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_minus, "page.zoom-out");
    add_shortcut(keys, GDK_KEY_KP_Subtract, "page.zoom-out");
    add_shortcut(keys, GDK_KEY_0, "page.zoom-reset");
    add_shortcut(keys, GDK_KEY_KP_0, "page.zoom-reset");
    gtk_widget_add_controller(root_, keys);
}

bool HelpPageView::scroll_to_heading(std::size_t index) {
    if (index >= heading_marks_.size()) {
        return false;
    }
    stop_heading_scroll();
    scroll_index_ = index;
    scroll_steady_ = 0;
    scroll_ticks_ = 0;
    scroll_waits_ = 0;
    scroll_source_ = g_timeout_add(kScrollTickMs, &HelpPageView::run_heading_scroll, this);
    run_heading_scroll(this); // one try at once
    return true;
}

void HelpPageView::stop_heading_scroll() {
    if (scroll_source_ != 0) {
        g_source_remove(scroll_source_);
        scroll_source_ = 0;
    }
}

gboolean HelpPageView::run_heading_scroll(gpointer data) {
    HelpPageView* self = static_cast<HelpPageView*>(data);
    if (!gtk_widget_get_mapped(self->root_)) {
        if (++self->scroll_waits_ >= kScrollMaxWaits) {
            self->scroll_source_ = 0;
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    }
    GtkTextIter at;
    gtk_text_buffer_get_iter_at_mark(self->buffer_, &at, self->heading_marks_[self->scroll_index_]);
    GdkRectangle where;
    gtk_text_view_get_iter_location(self->view_, &at, &where);
    GtkAdjustment* adjustment = self->vadjustment();
    const double last =
        gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);
    const double want =
        std::max(0.0, std::min(static_cast<double>(where.y + kHelpPageMargin), last));
    const double value = gtk_adjustment_get_value(adjustment);
    if (std::fabs(value - want) > 0.5) {
        gtk_adjustment_set_value(adjustment, want);
        self->scroll_steady_ = 0;
    } else {
        ++self->scroll_steady_;
    }
    if (self->scroll_steady_ >= kScrollSteadyTicks || ++self->scroll_ticks_ >= kScrollMaxTicks) {
        self->scroll_source_ = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

void HelpPageView::on_wheel(double dy) {
    GtkAdjustment* adjustment = vadjustment();
    const double step = std::pow(gtk_adjustment_get_page_size(adjustment), 2.0 / 3.0);
    gtk_adjustment_set_value(adjustment, gtk_adjustment_get_value(adjustment) + dy * step);
}

bool HelpPageView::set_zoom(double zoom) {
    const double clamped = std::min(kHelpMaxZoom, std::max(kHelpMinZoom, zoom));
    if (clamped == zoom_) {
        return false;
    }
    zoom_ = clamped;
    apply_zoom();
    return true;
}

bool HelpPageView::zoom_in() {
    for (double step : kZoomSteps) {
        if (step > zoom_ + 1e-9) {
            return set_zoom(step);
        }
    }
    return false;
}

bool HelpPageView::zoom_out() {
    for (std::size_t i = std::size(kZoomSteps); i > 0; --i) {
        if (kZoomSteps[i - 1] < zoom_ - 1e-9) {
            return set_zoom(kZoomSteps[i - 1]);
        }
    }
    return false;
}

bool HelpPageView::reset_zoom() {
    return set_zoom(1.0);
}

} // namespace vermilion::ui
