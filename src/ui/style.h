// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Installs the classic stylesheet (light palette) from the program's resources.

#pragma once

namespace vermilion::ui {

// Adds the stylesheet to the default display at APPLICATION priority, over the desktop's own
// theme. Never changes the theme name. Call once, after GTK is initialised and the resources are
// registered.
void install_classic_style();

} // namespace vermilion::ui
