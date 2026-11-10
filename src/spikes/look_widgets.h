// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: builds the sample window parts (documents, side panel, status bar).

#pragma once

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <vector>

namespace vermilion::spike {

// Each function returns a new floating widget; the container it is added to sinks it.
// Adds each page's buffer to buffers (borrowed pointers; each view keeps its buffer alive).
GtkWidget* build_documents(std::vector<GtkSourceBuffer*>& buffers);

GtkWidget* build_side_panel();

// Sets message to the status message label (borrowed; owned by the status bar).
GtkWidget* build_status_bar(GtkLabel** message);

} // namespace vermilion::spike
