// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Editor tab: builds its drop-down and maps it to the tab width.

#include "ui/settings_page_editor.h"

#include "ui/settings_page_layout.h"

namespace vermilion::ui {

unsigned EditorSettingsPage::tab_width_index(int width) {
    return width == 8 ? 1U : 0U;
}

int EditorSettingsPage::tab_width_at(unsigned index) {
    return index == 1 ? 8 : 4;
}

EditorSettingsPage::EditorSettingsPage() {
    column_ = settings_column(); // floating; the notebook of the window takes it over
    const char* const widths[] = {"4 spaces", "8 spaces", nullptr};
    dropdown_ = gtk_drop_down_new_from_strings(widths); // floating; the row takes it over
    gtk_box_append(GTK_BOX(column_), settings_row("_Tab width:", dropdown_));
    GtkWidget* note = gtk_label_new("The Tab key inserts this many spaces."); // floating
    gtk_label_set_xalign(GTK_LABEL(note), 0.0F);
    gtk_box_append(GTK_BOX(column_), note);

    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        dropdown_, "notify::selected", [this](GObject*, GParamSpec*) { notify_changed(); }));
}

void EditorSettingsPage::show_settings(const EditorSettings& settings) {
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown_), tab_width_index(settings.tab_width));
}

void EditorSettingsPage::store(EditorSettings& settings) const {
    settings.tab_width = tab_width_at(gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown_)));
}

} // namespace vermilion::ui
