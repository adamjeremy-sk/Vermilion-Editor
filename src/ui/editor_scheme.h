// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The style schemes of the editor text: the classic schemes, light and dark, with Vermilion's
// current-line colour.

#pragma once

#include <gtksourceview/gtksource.h>

#include <string>

namespace vermilion::ui {

// The scheme every editor buffer uses: "vermilion-classic" for `dark == false` and
// "vermilion-classic-dark" for `dark == true`. The first names "classic" as its parent and the
// second "classic-dark"; each overrides only the colours of the current line, so the cursor's line
// is visible on every monitor (Adam, 2026-11-16; the dark band 2026-11-19). GtkSourceView reads a
// scheme only from a real folder on disk (it cannot read one from the program's resources), so the
// first call writes both scheme files, whose texts are resources of the program, into
// scheme_folder() with write_scheme_files() and adds that folder to the scheme manager's search
// path (once; nothing else of the manager changes). If the files cannot be written, a warning is
// logged and the plain parent scheme is returned instead ("classic" or "classic-dark"); null when
// that does not exist either. The result is borrowed: the default GtkSourceStyleSchemeManager owns
// it, and the same `dark` always gives the same scheme. The program's resources must be registered
// before (vermilion_register_resource()). Needs no display. Main loop only.
GtkSourceStyleScheme* editor_scheme(bool dark);

// The folder that holds the scheme files: $XDG_CACHE_HOME/vermilion/styles (GLib's user cache
// folder, so the usual ~/.cache/vermilion/styles). It is only a copy: deleting it is harmless.
std::string scheme_folder();

// Makes sure that `folder` holds vermilion-classic.xml and vermilion-classic-dark.xml, each with
// exactly the text of the program's resource: creates the folder (and its parents, mode 0700) when
// it is missing, and writes each file (atomically, with g_file_set_contents) when it is missing or
// differs. A file that is current is not touched. Both files are always tried, even when the first
// one fails. Returns true when both files in `folder` are current afterwards, false when a
// resource is missing, the folder cannot be made or a file cannot be written. Logs nothing.
bool write_scheme_files(const std::string& folder);

} // namespace vermilion::ui
