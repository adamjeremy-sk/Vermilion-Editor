// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The shortcut table of the main window: which key starts which window action.

#include "ui/window_shortcuts.h"

namespace vermilion::ui {

namespace {

struct Shortcut {
    const char* action;
    const char* accels[3];
};

const Shortcut kShortcuts[] = {
    {"win.new-file", {"<Control>n", nullptr, nullptr}},
    {"win.open", {"<Control>o", nullptr, nullptr}},
    {"win.save", {"<Control>s", nullptr, nullptr}},
    {"win.save-as", {"<Control><Shift>s", nullptr, nullptr}},
    {"win.reload-from-disk", {"<Control>r", nullptr, nullptr}},
    {"win.close-tab", {"<Control>w", nullptr, nullptr}},
    {"win.quit", {"<Control>q", nullptr, nullptr}},
    {"win.undo", {"<Control>z", nullptr, nullptr}},
    {"win.redo", {"<Control>y", "<Control><Shift>z", nullptr}},
    {"win.cut", {"<Control>x", nullptr, nullptr}},
    {"win.copy", {"<Control>c", nullptr, nullptr}},
    {"win.paste", {"<Control>v", nullptr, nullptr}},
    {"win.select-all", {"<Control>a", nullptr, nullptr}},
    {"win.find", {"<Control>f", nullptr, nullptr}},
    {"win.find-next", {"F3", nullptr, nullptr}},
    {"win.find-previous", {"<Shift>F3", nullptr, nullptr}},
    {"win.replace", {"<Control>h", nullptr, nullptr}},
    {"win.go-to-line", {"<Control>g", nullptr, nullptr}},
    {"win.next-tab", {"<Control>Page_Down", nullptr, nullptr}},
    {"win.previous-tab", {"<Control>Page_Up", nullptr, nullptr}},
    {"win.settings", {"<Control>comma", nullptr, nullptr}},
    {"win.help-index", {"F1", nullptr, nullptr}},
};

} // namespace

void set_shortcuts(GtkApplication* app) {
    for (const Shortcut& shortcut : kShortcuts) {
        gtk_application_set_accels_for_action(app, shortcut.action, shortcut.accels);
    }
}

} // namespace vermilion::ui
