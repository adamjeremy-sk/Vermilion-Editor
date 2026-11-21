// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// SystemTheme: what the desktop says about light and dark, and a call when it changes.

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <optional>

#include "core/colour_mode.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// Gathers the SystemHints (core/colour_mode.h): the name of the GTK theme, which it follows live
// (a desktop that switches theme, such as XFCE, changes "gtk-theme-name"), and the answer of the
// desktop's settings portal (org.freedesktop.portal.Settings, "color-scheme"), which it asks once
// and follows through the portal's change signal. A desktop without the portal, or without its
// Settings interface, gives no answer and nothing is reported for it. The portal answers on the
// main loop, so the first answer comes after create() returns. Main loop only.
class SystemTheme {
public:
    // What is called with the new hints whenever the theme name or the portal's answer changes
    // (never from create(), and never when a new value equals the old one).
    using Changed = std::function<void(const SystemHints&)>;

    // The hints at this moment are in hints(). `prefer_dark_at_start` is the value that GTK's
    // "gtk-application-prefer-dark-theme" had before the program set it
    // (Style::prefer_dark_at_start, or false when there is no Style). `ask_portal` false never
    // talks to D-Bus (tests). Returns null, with a line in the log, when there is no display.
    // `changed` is not empty.
    static std::unique_ptr<SystemTheme> create(bool prefer_dark_at_start, bool ask_portal,
                                               Changed changed);

    // Disconnects from GTK, unsubscribes from the portal and cancels what is under way; nothing is
    // called after this.
    ~SystemTheme();

    SystemTheme(const SystemTheme&) = delete;
    SystemTheme& operator=(const SystemTheme&) = delete;

    // The hints as the desktop gave them last.
    const SystemHints& hints() const { return hints_; }

    // The answer of the portal's "color-scheme" value: the variant is the value itself, or the
    // value wrapped in one or more variants (the portal's Read wraps it twice). Value 1 is dark,
    // 2 is light, 0 means no preference; a variant of any other type gives nothing.
    // `value` is borrowed, may be null.
    static std::optional<bool> dark_from_portal_value(GVariant* value);

    // Takes a value that the portal gave (the reply of Read, or the signal): works out the answer
    // with dark_from_portal_value and calls `changed` when it differs from the one held. Public
    // for the tests; the D-Bus code calls it. `value` is borrowed, may be null.
    void set_portal_value(GVariant* value);

private:
    SystemTheme(GtkSettings* settings, bool prefer_dark_at_start, Changed changed);

    // Starts the portal conversation: connects to the session bus, subscribes to the change
    // signal, then reads the value once.
    void ask_portal();
    void on_bus(GDBusConnection* connection);
    void on_theme_name();
    void report();

    static void on_bus_ready(GObject* source, GAsyncResult* result, gpointer ticket);
    static void on_read_ready(GObject* source, GAsyncResult* result, gpointer ticket);
    static void on_setting_changed(GDBusConnection* connection, const gchar* sender,
                                   const gchar* path, const gchar* interface_name,
                                   const gchar* signal_name, GVariant* parameters, gpointer ticket);

    GtkSettings* settings_; // borrowed from GTK, lives as long as the display
    Changed changed_;
    SystemHints hints_;
    gtkx::SignalConnection theme_name_;
    gtkx::ObjectPtr<GCancellable> cancellable_;
    gtkx::ObjectPtr<GDBusConnection> bus_;
    guint subscription_ = 0;
    gtkx::AsyncGuard<SystemTheme> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
