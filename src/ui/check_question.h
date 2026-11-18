// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A modal question with a check box (GtkAlertDialog has none): two buttons and one tick box.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace vermilion::ui {

// Shows a small modal window over parent (borrowed) with a message, a detail line, a check box
// labelled check_label (not ticked at first) and two buttons: go_label and Cancel. Cancel is the
// default button and the answer of Escape and of the title bar's close button. on_answer runs
// exactly once, from the main loop: with (true, ticked) when the user chooses go_label, with
// (false, false) for every other way out. It never runs when parent is destroyed first (the window
// goes with its parent and nothing is answered). GTK owns the window; the caller keeps nothing.
void ask_with_check(GtkWindow* parent, const std::string& message, const std::string& detail,
                    const std::string& go_label, const std::string& check_label,
                    std::function<void(bool go, bool ticked)> on_answer);

} // namespace vermilion::ui
