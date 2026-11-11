// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the status bar panels and shows the active document's position and mode in them.

#include "ui/status_bar.h"

#include <string>

namespace vermilion::ui {

namespace {

// Appends a label with the given style class to bar; the bar owns it, the pointer is borrowed.
GtkWidget* add_panel(GtkWidget* bar, const char* css_class) {
    GtkWidget* label = gtk_label_new("");
    gtk_widget_add_css_class(label, css_class);
    gtk_box_append(GTK_BOX(bar), label);
    return label;
}

void set_text(GtkWidget* label, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
}

} // namespace

// Floating box from _new; adopt_floating sinks it, so this object keeps it (and its labels) alive.
StatusBar::StatusBar()
    : box_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2))) {
    gtk_widget_add_css_class(box_.get(), "vm-statusbar");
    GtkWidget* message = add_panel(box_.get(), "vm-status-message");
    gtk_label_set_text(GTK_LABEL(message), "Ready");
    gtk_label_set_xalign(GTK_LABEL(message), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(message), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(message, TRUE);
    language_ = add_panel(box_.get(), "vm-status-panel");
    tab_width_ = add_panel(box_.get(), "vm-status-panel");
    position_ = add_panel(box_.get(), "vm-status-panel");
    mode_ = add_panel(box_.get(), "vm-status-panel");
    show_no_document();
}

void StatusBar::show_document(const CursorStatus& status) {
    // Languages arrive with opening files (M2.2); every document is plain text until then.
    set_text(language_, "Plain Text");
    set_text(tab_width_, "Tab Width: " + std::to_string(status.tab_width));
    set_text(position_,
             "Ln " + std::to_string(status.line) + ", Col " + std::to_string(status.column));
    set_text(mode_, status.overwrite ? "OVR" : "INS");
    for (GtkWidget* panel : panels()) {
        gtk_widget_set_visible(panel, TRUE);
    }
}

void StatusBar::show_no_document() {
    for (GtkWidget* panel : panels()) {
        gtk_widget_set_visible(panel, FALSE);
    }
}

std::array<GtkWidget*, 4> StatusBar::panels() const {
    return {language_, tab_width_, position_, mode_};
}

} // namespace vermilion::ui
