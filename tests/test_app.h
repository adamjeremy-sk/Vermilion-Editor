// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The registered application that the display tests of one test program share.

#pragma once

#include <gtk/gtk.h>

#include <memory>

namespace test_app {

// A new application named `id`, registered on the session bus, or null if it cannot register.
// GLib keeps a registered application until the process ends, and a second one with the same name
// cannot register, so a fixture keeps the result in a static and all its tests share it.
inline GtkApplication* registered(const char* id) {
    std::unique_ptr<GtkApplication, void (*)(gpointer)> made(
        gtk_application_new(id, G_APPLICATION_NON_UNIQUE), &g_object_unref);
    if (!g_application_register(G_APPLICATION(made.get()), nullptr, nullptr)) {
        return nullptr;
    }
    return made.release(); // kept until the process ends
}

} // namespace test_app
