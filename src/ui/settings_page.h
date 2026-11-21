// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// SettingsPage: one tab of the Settings window, which shows and reads some of the settings.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <utility>

#include "core/editor_settings.h"

namespace vermilion::ui {

// A page owns its controls (check boxes, drop-downs, number boxes) and knows which fields of
// EditorSettings they show. The Settings window gives it the settings to show, asks it for the
// values the person chose, and learns from it that a control was changed. Main loop only.
class SettingsPage {
public:
    virtual ~SettingsPage() = default;

    SettingsPage(const SettingsPage&) = delete;
    SettingsPage& operator=(const SettingsPage&) = delete;

    // The widget of the page: a floating widget, which the window's notebook takes over (borrowed).
    // The same widget every time.
    virtual GtkWidget* widget() const = 0;

    // Puts the values of this page's fields into `settings`; every other field is left as it is.
    virtual void store(EditorSettings& settings) const = 0;

    // Shows the values of this page's fields in the controls. A control that changes because of
    // this call does not count as a change by the person: the function given to set_changed() is
    // not called while it runs.
    void load(const EditorSettings& settings) {
        loading_ = true;
        show_settings(settings);
        loading_ = false;
    }

    // The function to call when the person changes a control. Given by the window.
    void set_changed(std::function<void()> changed) { changed_ = std::move(changed); }

protected:
    SettingsPage() = default;

    // What load() does: sets the controls from the fields this page owns.
    virtual void show_settings(const EditorSettings& settings) = 0;

    // A page calls this from the signal of every control the person can change. Does nothing while
    // load() runs, or when no function was given.
    void notify_changed() {
        if (!loading_ && changed_) {
            changed_();
        }
    }

private:
    std::function<void()> changed_;
    bool loading_ = false;
};

} // namespace vermilion::ui
