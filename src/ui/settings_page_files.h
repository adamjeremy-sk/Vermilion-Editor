// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Files tab of the Settings window: the two questions that opening and reloading can ask.

#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "core/editor_settings.h"
#include "ui/gtkx/signal.h"
#include "ui/settings_page.h"

namespace vermilion::ui {

// The fields confirm_large_reload and long_line_limit of EditorSettings: a check box "Ask before
// reloading a large file (over 10 MB) from disk" and a number box for the length of a line, in
// characters, above which opening a file asks first. The number box runs from
// settings::kLongLineLimitMin to settings::kLongLineLimitMax.
class FilesSettingsPage : public SettingsPage {
public:
    FilesSettingsPage();

    GtkWidget* widget() const override { return column_; }
    void store(EditorSettings& settings) const override;

    // The controls (borrowed), for the tests.
    GtkWidget* confirm_check() const { return confirm_; }
    GtkWidget* limit_spin() const { return limit_; }

protected:
    void show_settings(const EditorSettings& settings) override;

private:
    GtkWidget* column_ = nullptr; // the widgets are borrowed; the page's widget owns them
    GtkWidget* confirm_ = nullptr;
    GtkWidget* limit_ = nullptr;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
