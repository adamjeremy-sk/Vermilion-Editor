// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the colours of the Help page view: the palette names read into the text tags.

#include <gtk/gtk.h>

#include <string>

#include "ui/help_page_view.h"

namespace vermilion::ui {
namespace {

// Puts the colour `name` of the view's style into the `property` ("foreground" or "background")
// of `tag`, or leaves that colour unset when the name is not in the style. The lookup is the
// deprecated gtk_style_context_lookup_color (deprecated since GTK 4.10), but it is the only call
// that reads a name given with @define-color.
void set_tag_colour(GtkWidget* view, GtkTextTag* tag, const char* property, const char* name) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkStyleContext* context = gtk_widget_get_style_context(view); // borrowed
    GdkRGBA colour;
    const bool found = gtk_style_context_lookup_color(context, name, &colour) != FALSE;
    G_GNUC_END_IGNORE_DEPRECATIONS
    const std::string rgba = std::string(property) + "-rgba";
    const std::string is_set = std::string(property) + "-set";
    if (found) {
        g_object_set(tag, rgba.c_str(), &colour, nullptr);
    } else {
        g_object_set(tag, is_set.c_str(), FALSE, nullptr);
    }
}

} // namespace

void HelpPageView::refresh_colours() {
    GtkWidget* view = GTK_WIDGET(view_);
    set_tag_colour(view, link_, "foreground", kColourLink);
    set_tag_colour(view, code_, "background", kColourCodeBg);
    set_tag_colour(view, table_header_, "background", kColourTableHeadBg);
    set_tag_colour(view, table_header_, "foreground", kColourTableHeadText);
}

} // namespace vermilion::ui
