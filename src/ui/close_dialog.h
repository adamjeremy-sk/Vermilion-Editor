// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Asks before unsaved changes are thrown away: one question with a discard button and Cancel.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

namespace vermilion::ui {

// Shows a modal question over parent. Cancel is the default and the Escape answer. on_discard
// runs only when the discard button is chosen and parent still exists.
void ask_discard(GtkWindow* parent, const std::string& message, const std::string& detail,
                 const std::string& discard_label, std::function<void()> on_discard);

} // namespace vermilion::ui
