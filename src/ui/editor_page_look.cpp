// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// EditorPage::apply_look: the scheme, the current line, the tab and the font class of the view.

#include "ui/editor_page.h"

#include "ui/editor_scheme.h"
#include "ui/font_css.h"

namespace vermilion::ui {

void EditorPage::apply_look(const EditorLook& look) {
    GtkSourceStyleScheme* scheme = editor_scheme(look.dark); // borrowed; may be null
    if (scheme != nullptr) {
        gtk_source_buffer_set_style_scheme(buffer_, scheme);
    }
    gtk_source_view_set_highlight_current_line(view_, look.highlight_current_line ? TRUE : FALSE);
    gtk_source_view_set_tab_width(view_, static_cast<guint>(look.tab_width));
    gtk_source_view_set_insert_spaces_instead_of_tabs(view_, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(view_), kEditorFontClass);
}

} // namespace vermilion::ui
