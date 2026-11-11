// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The main window's action table (menus and shortcuts share it) and the application shortcuts.

#pragma once

#include <gtk/gtk.h>

namespace vermilion::ui {

class MainWindow;

// Adds every window action ("win." prefix) to map; each one calls a method of window, which
// must outlive the actions (the window owns both).
void add_window_actions(GActionMap* map, MainWindow* window);

// Sets the shortcuts of ARCHITECTURE.md 7.10 on app.
void set_shortcuts(GtkApplication* app);

} // namespace vermilion::ui
