// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The keyboard shortcuts of the main window (ARCHITECTURE.md 7.10).

#pragma once

#include <gtk/gtk.h>

namespace vermilion::ui {

// Sets the shortcuts on app: one gtk_application_set_accels_for_action for every window action
// that has a key ("win." prefix). Needs no display. Call once, before the first window is created.
void set_shortcuts(GtkApplication* app);

} // namespace vermilion::ui
