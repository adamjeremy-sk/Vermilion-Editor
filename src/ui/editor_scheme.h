// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The style scheme of the editor text: the classic scheme with Vermilion's current-line colour.

#pragma once

#include <gtksourceview/gtksource.h>

#include <string>

namespace vermilion::ui {

// The scheme every editor buffer uses: "vermilion-classic", which names "classic" as its parent
// and overrides only the colours of the current line, so the cursor's line is visible on every
// monitor (Adam, 2026-11-16). GtkSourceView reads a scheme only from a real folder on disk (it
// cannot read one from the program's resources), so the first call writes the scheme file, whose
// text is a resource of the program, into scheme_folder() with write_scheme_file() and adds that
// folder to the scheme manager's search path (once; nothing else of the manager changes). If the
// file cannot be written, a warning is logged and the plain "classic" scheme is returned; null
// when neither exists. The result is borrowed: the default GtkSourceStyleSchemeManager owns it.
// The program's resources must be registered before (vermilion_register_resource()). Needs no
// display. Main loop only.
GtkSourceStyleScheme* editor_scheme();

// The folder that holds the scheme file: $XDG_CACHE_HOME/vermilion/styles (GLib's user cache
// folder, so the usual ~/.cache/vermilion/styles). It is only a copy: deleting it is harmless.
std::string scheme_folder();

// Makes sure that `folder` holds vermilion-classic.xml with exactly the text of the program's
// resource: creates the folder (and its parents, mode 0700) when it is missing, and writes the
// file (atomically, with g_file_set_contents) when it is missing or differs. A file that is
// current is not touched. Returns true when the file in `folder` is current afterwards, false
// when the resource is missing, the folder cannot be made or the file cannot be written. Logs
// nothing.
bool write_scheme_file(const std::string& folder);

} // namespace vermilion::ui
