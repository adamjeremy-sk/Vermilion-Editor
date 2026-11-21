// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The layout that every page of the Settings window shares: the column and the labelled row.

#pragma once

#include <gtk/gtk.h>

namespace vermilion::ui {

inline constexpr int kSettingsMargin = 12;
inline constexpr int kSettingsSpacing = 10;
// Characters: the controls of every labelled row start at the same place.
inline constexpr int kSettingsLabelChars = 12;

// The widget of a page: a vertical box with a margin on all four sides. Floating; the window's
// notebook takes it over.
inline GtkWidget* settings_column() {
    GtkWidget* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, kSettingsSpacing); // floating
    gtk_widget_set_margin_top(column, kSettingsMargin);
    gtk_widget_set_margin_bottom(column, kSettingsMargin);
    gtk_widget_set_margin_start(column, kSettingsMargin);
    gtk_widget_set_margin_end(column, kSettingsMargin);
    return column;
}

// A horizontal box with a label (its text has a mnemonic; the label activates `control`) and then
// `control`, which the box takes over. Floating.
inline GtkWidget* settings_row(const char* label_text, GtkWidget* control) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kSettingsSpacing); // floating
    GtkWidget* label = gtk_label_new_with_mnemonic(label_text);                 // floating
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), control);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_width_chars(GTK_LABEL(label), kSettingsLabelChars);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), control);
    return row;
}

} // namespace vermilion::ui
