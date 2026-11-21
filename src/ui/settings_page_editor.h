// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Editor tab of the Settings window: the width of a tab.

#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "core/editor_settings.h"
#include "ui/gtkx/signal.h"
#include "ui/settings_page.h"

namespace vermilion::ui {

// The field tab_width of EditorSettings, in a drop-down with "4 spaces" and "8 spaces". The Tab key
// inserts that many spaces (M2.5 design).
class EditorSettingsPage : public SettingsPage {
public:
    EditorSettingsPage();

    GtkWidget* widget() const override { return column_; }
    void store(EditorSettings& settings) const override;

    // Where each width stands in the drop-down: 4 is 0 and 8 is 1; any other width gives 0.
    static unsigned tab_width_index(int width);
    // The inverse: 1 gives 8; any other index gives 4.
    static int tab_width_at(unsigned index);

    // The control (borrowed), for the tests.
    GtkWidget* tab_width_dropdown() const { return dropdown_; }

protected:
    void show_settings(const EditorSettings& settings) override;

private:
    GtkWidget* column_ = nullptr; // the widgets are borrowed; the page's widget owns them
    GtkWidget* dropdown_ = nullptr;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
