// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds a tab label and turns its close button, middle click and mouse wheel into callbacks.

#include "ui/tab_label.h"

#include <utility>

namespace vermilion::ui {

namespace {

constexpr int kTitleChars = 18;
constexpr const char* kCloseSign = "\u00d7"; // multiplication sign; bold through the stylesheet

} // namespace

// Floating box from _new; adopt_floating sinks it, so this object keeps it alive while the
// notebook also holds it.
TabLabel::TabLabel(Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      box_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4))) {
    title_ = gtk_label_new("");
    gtk_label_set_width_chars(GTK_LABEL(title_), kTitleChars);
    gtk_label_set_max_width_chars(GTK_LABEL(title_), kTitleChars);
    gtk_label_set_ellipsize(GTK_LABEL(title_), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(title_), 0.0f);
    // Fixed width and no hexpand: an expanding child would make the notebook stretch every tab
    // across the strip. The close button follows the title, at the tab's right edge.
    gtk_box_append(GTK_BOX(box_.get()), title_);

    close_ = gtk_button_new_with_label(kCloseSign);
    gtk_widget_add_css_class(close_, "vm-tab-close");
    gtk_widget_set_valign(close_, GTK_ALIGN_CENTER);
    gtk_widget_set_focusable(close_, FALSE);
    gtk_widget_set_focus_on_click(close_, FALSE);
    gtk_widget_set_tooltip_text(close_, "Close Tab");
    gtk_box_append(GTK_BOX(box_.get()), close_);
    clicked_ = gtkx::connect<void(GtkButton*)>(close_, "clicked",
                                               [this](GtkButton*) { callbacks_.close(); });

    // gtk_widget_add_controller takes the full reference that _new returned (transfer full).
    GtkGesture* middle = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(middle), GDK_BUTTON_MIDDLE);
    gtk_widget_add_controller(box_.get(), GTK_EVENT_CONTROLLER(middle));
    middle_released_ = gtkx::connect<void(GtkGestureClick*, int, double, double)>(
        middle, "released", [this](GtkGestureClick*, int, double, double) { callbacks_.close(); });

    GtkEventController* wheel =
        gtk_event_controller_scroll_new(static_cast<GtkEventControllerScrollFlags>(
            GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE));
    gtk_widget_add_controller(box_.get(), wheel);
    wheel_ = gtkx::connect<gboolean(GtkEventControllerScroll*, double, double)>(
        wheel, "scroll", [this](GtkEventControllerScroll*, double, double dy) -> gboolean {
            if (dy == 0.0) {
                return FALSE;
            }
            callbacks_.scroll(dy > 0.0 ? 1 : -1);
            return TRUE;
        });
}

void TabLabel::set(const std::string& title, const std::string& tooltip, bool modified) {
    std::string text = modified ? "*" + title : title;
    gtk_label_set_text(GTK_LABEL(title_), text.c_str());
    gtk_widget_set_tooltip_text(box_.get(), tooltip.c_str());
}

bool TabLabel::contains_close_button(GtkWidget* widget) const {
    return widget != nullptr && (widget == close_ || gtk_widget_is_ancestor(widget, close_));
}

} // namespace vermilion::ui
