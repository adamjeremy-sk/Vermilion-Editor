// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// SystemTheme: the GTK theme name and the settings portal's colour scheme.

#include "ui/system_theme.h"

#include <string>
#include <utility>

#include "core/log.h"

namespace vermilion::ui {

namespace {

constexpr char kPortalName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalSettings[] = "org.freedesktop.portal.Settings";
constexpr char kNamespace[] = "org.freedesktop.appearance";
constexpr char kKey[] = "color-scheme";

std::string theme_name_of(GtkSettings* settings) {
    gchar* raw = nullptr;
    g_object_get(settings, "gtk-theme-name", &raw, nullptr);
    std::unique_ptr<gchar, decltype(&g_free)> owned(raw, &g_free);
    return owned ? owned.get() : "";
}

} // namespace

std::unique_ptr<SystemTheme> SystemTheme::create(bool prefer_dark_at_start, bool ask_portal,
                                                 Changed changed) {
    GtkSettings* settings = gtk_settings_get_default(); // borrowed; null without a display
    if (settings == nullptr) {
        logging::warning("SystemTheme: no display, the desktop's colour scheme is not followed");
        return nullptr;
    }
    std::unique_ptr<SystemTheme> theme(
        new SystemTheme(settings, prefer_dark_at_start, std::move(changed)));
    if (ask_portal) {
        theme->ask_portal();
    }
    return theme;
}

SystemTheme::SystemTheme(GtkSettings* settings, bool prefer_dark_at_start, Changed changed)
    : settings_(settings), changed_(std::move(changed)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())) {
    hints_.theme_name = theme_name_of(settings_);
    hints_.prefer_dark_at_start = prefer_dark_at_start;
    theme_name_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        settings_, "notify::gtk-theme-name", [this](GObject*, GParamSpec*) { on_theme_name(); });
}

SystemTheme::~SystemTheme() {
    g_cancellable_cancel(cancellable_.get());
    if (bus_ && subscription_ != 0) {
        g_dbus_connection_signal_unsubscribe(bus_.get(), subscription_);
    }
}

std::optional<bool> SystemTheme::dark_from_portal_value(GVariant* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
        GVariant* inner = g_variant_get_variant(value); // new reference
        std::optional<bool> answer = dark_from_portal_value(inner);
        g_variant_unref(inner);
        return answer;
    }
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
        return portal_dark_from_color_scheme(g_variant_get_uint32(value));
    }
    return std::nullopt;
}

void SystemTheme::set_portal_value(GVariant* value) {
    std::optional<bool> answer = dark_from_portal_value(value);
    if (answer == hints_.portal_dark) {
        return;
    }
    hints_.portal_dark = answer;
    report();
}

void SystemTheme::on_theme_name() {
    std::string name = theme_name_of(settings_);
    if (name == hints_.theme_name) {
        return;
    }
    hints_.theme_name = std::move(name);
    report();
}

void SystemTheme::report() {
    changed_(hints_);
}

void SystemTheme::ask_portal() {
    g_bus_get(G_BUS_TYPE_SESSION, cancellable_.get(), &SystemTheme::on_bus_ready, guard_.ticket());
}

void SystemTheme::on_bus_ready(GObject*, GAsyncResult* result, gpointer ticket) {
    GError* error = nullptr;
    GDBusConnection* made = g_bus_get_finish(result, &error);
    if (error != nullptr) {
        g_error_free(error);
    }
    SystemTheme* self = gtkx::AsyncGuard<SystemTheme>::take(ticket);
    gtkx::ObjectPtr<GDBusConnection> connection = gtkx::ObjectPtr<GDBusConnection>::adopt(made);
    if (self == nullptr || !connection) {
        return;
    }
    self->on_bus(connection.get());
}

void SystemTheme::on_bus(GDBusConnection* connection) {
    bus_ = gtkx::ObjectPtr<GDBusConnection>::ref(connection);
    // Subscribe first, so a change between the subscribe and the read is not lost.
    subscription_ = g_dbus_connection_signal_subscribe(
        bus_.get(), kPortalName, kPortalSettings, "SettingChanged", kPortalPath, kNamespace,
        G_DBUS_SIGNAL_FLAGS_NONE, &SystemTheme::on_setting_changed, guard_.ticket(),
        &gtkx::AsyncGuard<SystemTheme>::free_ticket);
    g_dbus_connection_call(bus_.get(), kPortalName, kPortalPath, kPortalSettings, "Read",
                           g_variant_new("(ss)", kNamespace, kKey), G_VARIANT_TYPE("(v)"),
                           G_DBUS_CALL_FLAGS_NONE, -1, cancellable_.get(),
                           &SystemTheme::on_read_ready, guard_.ticket());
}

void SystemTheme::on_read_ready(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
    if (error != nullptr) {
        g_error_free(error);
    }
    SystemTheme* self = gtkx::AsyncGuard<SystemTheme>::take(ticket);
    if (self == nullptr || reply == nullptr) {
        if (reply != nullptr) {
            g_variant_unref(reply);
        }
        return;
    }
    GVariant* value = g_variant_get_child_value(reply, 0); // new reference
    self->set_portal_value(value);
    g_variant_unref(value);
    g_variant_unref(reply);
}

void SystemTheme::on_setting_changed(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                     const gchar*, GVariant* parameters, gpointer ticket) {
    SystemTheme* self = gtkx::AsyncGuard<SystemTheme>::peek(ticket);
    if (self == nullptr || parameters == nullptr ||
        !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ssv)"))) {
        return;
    }
    const gchar* ns = nullptr;
    const gchar* key = nullptr;
    GVariant* value = nullptr;
    g_variant_get(parameters, "(&s&sv)", &ns, &key, &value);
    if (std::string(ns) == kNamespace && std::string(key) == kKey) {
        self->set_portal_value(value);
    }
    g_variant_unref(value);
}

} // namespace vermilion::ui
