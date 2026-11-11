// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the status bar panels and shows the active document's position, format and mode in them.

#include "ui/status_bar.h"

#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

namespace {

constexpr const char* kMenuResource = "/io/github/adamjeremy_sk/Vermilion/menus.ui";

// Appends a label with the given style class to bar; the bar owns it, the pointer is borrowed.
GtkWidget* add_panel(GtkWidget* bar, const char* css_class) {
    GtkWidget* label = gtk_label_new("");
    gtk_widget_add_css_class(label, css_class);
    gtk_box_append(GTK_BOX(bar), label);
    return label;
}

// Appends a menu button that opens the menu named menu_id from the menus resource; the bar owns it.
GtkWidget* add_menu_panel(GtkWidget* bar, const char* menu_id) {
    GtkWidget* button = gtk_menu_button_new(); // floating; gtk_box_append takes it
    gtk_widget_add_css_class(button, "vm-status-panel");
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(button), GTK_ARROW_UP);
    auto builder = gtkx::ObjectPtr<GtkBuilder>::adopt(gtk_builder_new_from_resource(kMenuResource));
    GObject* menu = gtk_builder_get_object(builder.get(), menu_id); // borrowed
    if (menu != nullptr) {
        gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(button), G_MENU_MODEL(menu));
    }
    gtk_box_append(GTK_BOX(bar), button);
    return button;
}

void set_text(GtkWidget* label, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
}

void set_button_text(GtkWidget* button, const std::string& text) {
    gtk_menu_button_set_label(GTK_MENU_BUTTON(button), text.c_str());
}

} // namespace

// Floating box from _new; adopt_floating sinks it, so this object keeps it (and its labels) alive.
StatusBar::StatusBar()
    : box_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2))) {
    gtk_widget_add_css_class(box_.get(), "vm-statusbar");
    message_ = add_panel(box_.get(), "vm-status-message");
    gtk_label_set_text(GTK_LABEL(message_), "Ready");
    gtk_label_set_xalign(GTK_LABEL(message_), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(message_), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(message_, TRUE);
    language_ = add_panel(box_.get(), "vm-status-panel");
    tab_width_ = add_panel(box_.get(), "vm-status-panel");
    encoding_ = add_menu_panel(box_.get(), "encoding-menu");
    line_ending_ = add_menu_panel(box_.get(), "line-ending-menu");
    position_ = add_panel(box_.get(), "vm-status-panel");
    mode_ = add_panel(box_.get(), "vm-status-panel");
    show_no_document();
}

void StatusBar::show_document(const CursorStatus& status) {
    set_text(language_, status.language);
    set_text(tab_width_, "Tab Width: " + std::to_string(status.tab_width));
    set_button_text(encoding_, status.encoding);
    set_button_text(line_ending_, status.line_ending);
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

void StatusBar::show_message(const std::string& message) {
    set_text(message_, message);
}

std::array<GtkWidget*, 6> StatusBar::panels() const {
    return {language_, tab_width_, encoding_, line_ending_, position_, mode_};
}

} // namespace vermilion::ui
