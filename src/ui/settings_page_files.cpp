// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Files tab: builds its check box and number box and maps them to the settings.

#include "ui/settings_page_files.h"

#include <cstdint>

#include "core/settings_keys.h"
#include "ui/settings_page_layout.h"

namespace vermilion::ui {
namespace {

constexpr double kLimitStep = 10000.0;
constexpr double kLimitPageStep = 100000.0;

} // namespace

FilesSettingsPage::FilesSettingsPage() {
    column_ = settings_column(); // floating; the notebook of the window takes it over
    confirm_ = gtk_check_button_new_with_mnemonic(
        "_Ask before reloading a large file (over 10 MB) from disk"); // floating
    gtk_box_append(GTK_BOX(column_), confirm_);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kSettingsSpacing); // floating
    GtkWidget* lead =
        gtk_label_new("Ask before opening a file with a line longer than"); // floating
    limit_ = gtk_spin_button_new_with_range(settings::kLongLineLimitMin,
                                            settings::kLongLineLimitMax, kLimitStep); // floating
    gtk_spin_button_set_increments(GTK_SPIN_BUTTON(limit_), kLimitStep, kLimitPageStep);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(limit_), TRUE);
    GtkWidget* tail = gtk_label_new("characters"); // floating
    gtk_box_append(GTK_BOX(row), lead);
    gtk_box_append(GTK_BOX(row), limit_);
    gtk_box_append(GTK_BOX(row), tail);
    gtk_box_append(GTK_BOX(column_), row);

    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        confirm_, "toggled", [this](GtkCheckButton*) { notify_changed(); }));
    signals_.push_back(gtkx::connect<void(GtkSpinButton*)>(
        limit_, "value-changed", [this](GtkSpinButton*) { notify_changed(); }));
}

void FilesSettingsPage::show_settings(const EditorSettings& settings) {
    gtk_check_button_set_active(GTK_CHECK_BUTTON(confirm_), settings.confirm_large_reload);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(limit_),
                              static_cast<double>(settings.long_line_limit));
}

void FilesSettingsPage::store(EditorSettings& settings) const {
    settings.confirm_large_reload =
        gtk_check_button_get_active(GTK_CHECK_BUTTON(confirm_)) != FALSE;
    settings.long_line_limit =
        static_cast<std::uint64_t>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(limit_)));
}

} // namespace vermilion::ui
