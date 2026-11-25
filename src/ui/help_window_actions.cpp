// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The actions of the Help window: Back, Forward and the three Search actions, the keys of Back and
// Forward, and when Back and Forward can be used.

#include <gtk/gtk.h>

#include "ui/help_window.h"

namespace vermilion::ui {
namespace {

HelpWindow* self(gpointer data) {
    return static_cast<HelpWindow*>(data);
}

void on_back(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->back();
}

void on_forward(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->forward();
}

void on_find(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->show_search();
}

void on_find_next(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->step_match(true);
}

void on_find_previous(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->step_match(false);
}

const GActionEntry kActions[] = {
    {"back", on_back, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"forward", on_forward, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find", on_find, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find-next", on_find_next, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find-previous", on_find_previous, nullptr, nullptr, nullptr, {0, 0, 0}},
};

// The keys of Back and Forward. The Search actions have the editor's names, so their keys come
// from the program's shortcut table (window_shortcuts.cpp).
struct HelpKey {
    const char* action;
    const char* accels[2];
};

const HelpKey kKeys[] = {
    {"win.back", {"<Alt>Left", nullptr}},
    {"win.forward", {"<Alt>Right", nullptr}},
};

// Turns the window's action `name` on or off.
void set_action_enabled(GtkWindow* window, const char* name, bool enabled) {
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(window), name); // borrowed
    if (action != nullptr) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled ? TRUE : FALSE);
    }
}

} // namespace

void HelpWindow::add_actions(GtkApplication* app) {
    g_action_map_add_action_entries(G_ACTION_MAP(window_), kActions, G_N_ELEMENTS(kActions), this);
    for (const HelpKey& key : kKeys) {
        gtk_application_set_accels_for_action(app, key.action, key.accels);
    }
}

void HelpWindow::update_actions() {
    set_action_enabled(window_, "back", history_.can_go_back());
    set_action_enabled(window_, "forward", history_.can_go_forward());
}

} // namespace vermilion::ui
