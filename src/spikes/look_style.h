// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: shared state, the "Follow system" rule and applying the stylesheet and scheme.

#pragma once

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <memory>
#include <string>
#include <vector>

#include "spikes/look_portal.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::spike {

enum class Mode { Light, Dark, Follow };

struct LookState {
    GtkApplication* app = nullptr;         // borrowed; owned by main
    GtkWindow* window = nullptr;           // borrowed; owned by GTK's toplevel list
    GtkLabel* message = nullptr;           // borrowed; owned by the status bar
    std::vector<GtkSourceBuffer*> buffers; // borrowed; each owned by its view
    gtkx::ObjectPtr<GtkCssProvider> provider;
    gtkx::SignalConnection css_error;
    gtkx::SignalConnection theme_notify;
    gtkx::SignalConnection prefer_notify;
    gtkx::SignalConnection window_destroy;
    std::unique_ptr<PortalWatcher> portal;
    std::string data_dir;             // holds the stylesheets and the menu file
    std::string desktop_theme;        // last theme name seen while the spike did not pin it
    bool desktop_prefer_dark = false; // last prefer-dark value seen while not pinned
    bool prefer_dark_pinned = false;  // the spike set prefer-dark itself
    bool theme_pinned = false;        // the spike set the theme name to "Default" itself
    PortalScheme portal_scheme = PortalScheme::Pending;
    Mode mode = Mode::Follow;
    bool base_default = false; // true: GTK's built-in "Default" theme; false: the desktop theme
    bool applying = false;     // true while the spike itself changes GtkSettings
};

std::string read_theme_name(GtkSettings* settings);

bool read_prefer_dark(GtkSettings* settings);

// Changes GtkSettings only where the wanted state differs from what the spike has set, re-reads
// the desktop values it has not overridden, applies the look and prints one "LOOK <why> ..." line.
void apply(LookState& s, const char* why);

} // namespace vermilion::spike
