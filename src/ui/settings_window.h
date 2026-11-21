// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Settings window: tabs of settings, with OK, Cancel and Apply.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/editor_settings.h"
#include "ui/gtkx/signal.h"
#include "ui/settings_page.h"

namespace vermilion::ui {

// A modal window over the main window (design of M2.5, Adam, 2026-11-19): a notebook of pages, and
// the buttons OK, Cancel and Apply. It keeps a working copy of the settings: every change of a
// control updates it, nothing reaches the program before Apply or OK, Cancel throws it away. Apply
// is disabled while the working copy equals what was applied last (or, at the start, what the
// settings file held). The window is made once and hidden when it closes. Main loop only.
class SettingsWindow {
public:
    // What the window needs from the main window.
    struct Host {
        // The settings as the file holds them now; read at every open().
        std::function<EditorSettings()> current;
        // The person applied these settings (Apply, or OK): the host saves them and brings the
        // program up to date. Called only when they differ from what was applied last.
        std::function<void(const EditorSettings&)> apply;
        // The window was closed (OK, Cancel, Esc or the title bar). It is already hidden.
        std::function<void()> closed;
    };

    // Builds the window, hidden, as a modal transient child of parent (borrowed; the window is
    // destroyed with it). GTK owns the window; this object must be destroyed when the window emits
    // "destroy".
    SettingsWindow(GtkWindow* parent, Host host);

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    GtkWindow* window() const { return window_; }

    // True while the window is shown.
    bool is_open() const;

    // Adds a tab after the ones that are there; the window owns the page and gives it its
    // set_changed function. Call it before the first open().
    void add_page(const std::string& title, std::unique_ptr<SettingsPage> page);

    // The number of tabs.
    int page_count() const;

    // Shows the window. When it was hidden: reads Host::current, normalises it
    // (normalise_settings), makes it the applied settings and the working copy, gives it to every
    // page (SettingsPage::load) and disables Apply. When it is shown already: only brings it
    // forward, so the changes in it stay.
    void open();

    // What the buttons do. Each is also a public function so that the tests can call it.
    // OK: apply(), then closes. Apply: when the working copy differs from the applied settings,
    // calls Host::apply with it, makes it the applied settings and disables Apply; the window
    // stays open. Cancel: closes without applying; the changes are lost.
    void ok();
    void apply();
    void cancel();

    // The settings as the pages show them now: the applied settings with every page's fields
    // stored into them, normalised.
    const EditorSettings& working() const { return working_; }

    // The buttons (borrowed).
    GtkWidget* ok_button() const { return ok_; }
    GtkWidget* cancel_button() const { return cancel_; }
    GtkWidget* apply_button() const { return apply_; }

private:
    void build(GtkWindow* parent);
    void connect_signals();
    void connect_keys();
    // A control of some page changed: recomputes the working copy and enables Apply when it
    // differs from the applied settings.
    void on_page_changed();
    // Hides the window and calls Host::closed. Does nothing when the window is hidden already.
    void close();

    Host host_;
    GtkWindow* window_ = nullptr;   // borrowed; GTK owns the window
    GtkWidget* notebook_ = nullptr; // the widgets below are borrowed; the window owns them
    GtkWidget* ok_ = nullptr;
    GtkWidget* cancel_ = nullptr;
    GtkWidget* apply_ = nullptr;
    std::vector<std::unique_ptr<SettingsPage>> pages_;
    EditorSettings applied_;
    EditorSettings working_;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
