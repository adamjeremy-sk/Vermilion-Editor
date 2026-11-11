// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Questions before something is lost: a modal dialog with chosen buttons, the last one Cancel.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

namespace vermilion::ui {

// Shows a modal question over parent with the given buttons; the last button is Cancel, the
// default and the Escape answer. on_answer gets the index of the chosen button (the last one when
// the dialog is dismissed); it never runs once parent is gone.
void ask(GtkWindow* parent, const std::string& message, const std::string& detail,
         const std::vector<std::string>& buttons, std::function<void(int)> on_answer);

// A discard button and Cancel; on_discard runs only for the discard button.
void ask_discard(GtkWindow* parent, const std::string& message, const std::string& detail,
                 const std::string& discard_label, std::function<void()> on_discard);

} // namespace vermilion::ui
