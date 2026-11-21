// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The bundled font, Fantasque Sans Mono: writes its four files from the program's resources to the
// cache folder and makes them known to fontconfig and Pango, so GTK can use the family by name.

#pragma once

#include <string>
#include <vector>

namespace vermilion::ui {

// The family name as Pango and fontconfig list it.
inline constexpr char kBundledFontFamily[] = "Fantasque Sans Mono";

// Where the four files are in the program's resources (a GResource folder).
inline constexpr char kBundledFontResourceDir[] = "/io/github/adamjeremy_sk/Vermilion/fonts";

// The four file names, in this order: Regular, Bold, Italic, Bold Italic.
const std::vector<std::string>& bundled_font_files();

// $XDG_CACHE_HOME/vermilion/fonts (g_get_user_cache_dir), no trailing slash.
std::string bundled_font_folder();

// Writes each file from the resources into `folder` with vermilion::write_if_changed. Returns the
// full paths (folder/name) of the files that are in place (written or already right), in the order
// of bundled_font_files(); a file that is missing from the resources or could not be written is
// left out. Registers nothing.
std::vector<std::string> write_bundled_font(const std::string& folder);

// Adds each path to fontconfig's application fonts (FcConfigAppFontAddFile on the current
// configuration) and, when at least one was accepted, tells Pango's default font map that the
// configuration changed (pango_fc_font_map_config_changed on pango_cairo_font_map_get_default())
// so the family can be found. Returns how many paths fontconfig accepted. An empty list, or paths
// of files that do not exist, give 0 and change nothing. Needs no display.
int register_font_files(const std::vector<std::string>& paths);

// The program's one call: write_bundled_font(bundled_font_folder()), then register_font_files on
// the result. The work is done once per process; a later call returns the first call's count.
// Logs one g_warning that names the folder when fewer than four files were registered (the text
// then falls back to the system monospace font). Returns how many files were registered (0 to 4).
// Run it after vermilion_register_resource() and before the first window is made.
int install_bundled_font();

} // namespace vermilion::ui
