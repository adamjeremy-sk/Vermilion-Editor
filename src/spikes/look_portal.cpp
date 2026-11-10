// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: async D-Bus reads of org.freedesktop.appearance color-scheme from the portal.

#include "spikes/look_portal.h"

#include <cstring>
#include <memory>
#include <utility>

namespace vermilion::spike {

namespace {

constexpr const char* kNamespace = "org.freedesktop.appearance";
constexpr const char* kKey = "color-scheme";

using VariantPtr = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

bool is_cancelled(const GError* error) {
    return error != nullptr && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

} // namespace

const char* portal_scheme_name(PortalScheme scheme) {
    switch (scheme) {
    case PortalScheme::Pending:
        return "pending";
    case PortalScheme::Unavailable:
        return "unavailable";
    case PortalScheme::NoPreference:
        return "no-preference";
    case PortalScheme::Dark:
        return "dark";
    case PortalScheme::Light:
        return "light";
    case PortalScheme::Other:
        return "other";
    }
    return "other";
}

PortalScheme portal_scheme_from_value(GVariant* value) {
    if (value == nullptr) {
        return PortalScheme::Unavailable;
    }
    VariantPtr current(g_variant_ref(value), &g_variant_unref);
    while (g_variant_is_of_type(current.get(), G_VARIANT_TYPE_VARIANT)) {
        current = VariantPtr(g_variant_get_variant(current.get()), &g_variant_unref);
    }
    if (!g_variant_is_of_type(current.get(), G_VARIANT_TYPE_UINT32)) {
        return PortalScheme::Other;
    }
    switch (g_variant_get_uint32(current.get())) {
    case 0:
        return PortalScheme::NoPreference;
    case 1:
        return PortalScheme::Dark;
    case 2:
        return PortalScheme::Light;
    default:
        return PortalScheme::Other;
    }
}

PortalWatcher::PortalWatcher(Callback callback)
    : callback_(std::move(callback)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())) {}

PortalWatcher::~PortalWatcher() {
    // Pending calls finish with G_IO_ERROR_CANCELLED and then never touch this object.
    g_cancellable_cancel(cancellable_.get());
}

void PortalWatcher::start() {
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
                             "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                             "org.freedesktop.portal.Settings", cancellable_.get(),
                             &PortalWatcher::on_proxy_ready, this);
}

void PortalWatcher::on_proxy_ready(GObject* source, GAsyncResult* result, gpointer user_data) {
    (void)source;
    GError* raw_error = nullptr;
    GDBusProxy* proxy = g_dbus_proxy_new_for_bus_finish(result, &raw_error);
    ErrorPtr error(raw_error, &g_error_free);
    if (is_cancelled(error.get())) {
        return;
    }
    auto* self = static_cast<PortalWatcher*>(user_data);
    if (proxy == nullptr) {
        self->callback_(PortalScheme::Unavailable,
                        error ? error->message : "proxy creation failed");
        return;
    }
    self->proxy_ = gtkx::ObjectPtr<GDBusProxy>::adopt(proxy);
    self->signal_ = gtkx::connect<void(GDBusProxy*, const char*, const char*, GVariant*)>(
        self->proxy_.get(), "g-signal",
        [self](GDBusProxy*, const char*, const char* signal_name, GVariant* parameters) {
            self->on_signal(signal_name, parameters);
        });
    g_dbus_proxy_call(self->proxy_.get(), "Read", g_variant_new("(ss)", kNamespace, kKey),
                      G_DBUS_CALL_FLAGS_NONE, 2000, self->cancellable_.get(),
                      &PortalWatcher::on_read_done, self);
}

void PortalWatcher::on_read_done(GObject* source, GAsyncResult* result, gpointer user_data) {
    GError* raw_error = nullptr;
    VariantPtr reply(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &raw_error),
                     &g_variant_unref);
    ErrorPtr error(raw_error, &g_error_free);
    if (is_cancelled(error.get())) {
        return;
    }
    auto* self = static_cast<PortalWatcher*>(user_data);
    if (!reply) {
        self->callback_(PortalScheme::Unavailable, error ? error->message : "Read failed");
        return;
    }
    if (!g_variant_is_of_type(reply.get(), G_VARIANT_TYPE("(v)"))) {
        self->callback_(PortalScheme::Other, g_variant_get_type_string(reply.get()));
        return;
    }
    VariantPtr value(g_variant_get_child_value(reply.get(), 0), &g_variant_unref);
    self->callback_(portal_scheme_from_value(value.get()), "Read");
}

void PortalWatcher::on_signal(const char* signal_name, GVariant* parameters) {
    if (signal_name == nullptr || std::strcmp(signal_name, "SettingChanged") != 0) {
        return;
    }
    if (parameters == nullptr || !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ssv)"))) {
        return;
    }
    const char* name_space = nullptr;
    const char* key = nullptr;
    g_variant_get_child(parameters, 0, "&s", &name_space);
    g_variant_get_child(parameters, 1, "&s", &key);
    if (std::strcmp(name_space, kNamespace) != 0 || std::strcmp(key, kKey) != 0) {
        return;
    }
    VariantPtr value(g_variant_get_child_value(parameters, 2), &g_variant_unref);
    callback_(portal_scheme_from_value(value.get()), "SettingChanged");
}

} // namespace vermilion::spike
