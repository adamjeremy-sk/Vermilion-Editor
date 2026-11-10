// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: watches the desktop settings portal's colour-scheme value over D-Bus.

#pragma once

#include <gio/gio.h>

#include <functional>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::spike {

// What the portal said; Pending until the first answer, Unavailable if it could not be read.
enum class PortalScheme { Pending, Unavailable, NoPreference, Dark, Light, Other };

const char* portal_scheme_name(PortalScheme scheme);

// Maps the portal value (uint32: 0 no preference, 1 dark, 2 light), unwrapping nested variants.
// The value is borrowed and must not be floating.
PortalScheme portal_scheme_from_value(GVariant* value);

class PortalWatcher {
public:
    using Callback = std::function<void(PortalScheme scheme, const std::string& note)>;

    explicit PortalWatcher(Callback callback);
    ~PortalWatcher();

    PortalWatcher(const PortalWatcher&) = delete;
    PortalWatcher& operator=(const PortalWatcher&) = delete;

    // Creates the proxy asynchronously, reads the value once and then follows SettingChanged.
    void start();

private:
    static void on_proxy_ready(GObject* source, GAsyncResult* result, gpointer user_data);
    static void on_read_done(GObject* source, GAsyncResult* result, gpointer user_data);
    void on_signal(const char* signal_name, GVariant* parameters);

    Callback callback_;
    gtkx::ObjectPtr<GCancellable> cancellable_;
    gtkx::ObjectPtr<GDBusProxy> proxy_;
    gtkx::SignalConnection signal_;
};

} // namespace vermilion::spike
