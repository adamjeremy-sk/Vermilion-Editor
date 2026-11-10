// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: sample widgets for judging the classic stylesheet; nothing here does real work.

#include "spikes/look_widgets.h"

#include <cstring>
#include <string>

#include "ui/gtkx/object_ptr.h"

namespace vermilion::spike {

namespace {

constexpr const char* kSample = "// Sample text for the look spike.\n"
                                "#include <string>\n"
                                "\n"
                                "int main() {\n"
                                "    std::string greeting = \"Hello, Vermilion\";\n"
                                "    return 0; // selection and current line are visible\n"
                                "}\n";

GtkWidget* build_tab_label(const char* name, bool modified) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    std::string text = modified ? std::string("*") + name : std::string(name);
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(label), 18);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 18);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_tooltip_text(label, name);
    // The label takes any spare width, so the close button sits at the tab's right edge.
    GtkWidget* close = gtk_button_new_with_label("x");
    gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    gtk_widget_set_valign(close, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(close, "vm-tab-close");
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), close);
    return box;
}

} // namespace

GtkWidget* build_documents(std::vector<GtkSourceBuffer*>& buffers) {
    GtkWidget* notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    const char* const kNames[] = {"main.cpp",  "a_rather_long_file_name_for_tabs.txt",
                                  "notes.txt", "CMakeLists.txt",
                                  "README.md", "window.cpp",
                                  "window.h",  "Untitled 1"};
    bool first = true;
    for (const char* name : kNames) {
        // Full reference from _new; the view takes its own, so ours is dropped at scope end.
        auto buffer = gtkx::ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
        GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer.get());
        gtk_text_buffer_set_text(text, kSample, -1);
        if (first) {
            GtkSourceLanguageManager* manager = gtk_source_language_manager_get_default();
            GtkSourceLanguage* language = gtk_source_language_manager_get_language(manager, "cpp");
            if (language != nullptr) {
                gtk_source_buffer_set_language(buffer.get(), language);
            }
            GtkTextIter start;
            GtkTextIter end;
            gtk_text_buffer_get_iter_at_line(text, &start, 4);
            gtk_text_buffer_get_iter_at_line(text, &end, 5);
            gtk_text_buffer_select_range(text, &start, &end);
        }
        GtkWidget* view = gtk_source_view_new_with_buffer(buffer.get());
        gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
        gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), TRUE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
        GtkWidget* scrolled = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled,
                                 build_tab_label(name, std::strcmp(name, "notes.txt") == 0));
        gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), scrolled, TRUE);
        buffers.push_back(buffer.get());
        first = false;
    }
    return notebook;
}

GtkWidget* build_side_panel() {
    GtkWidget* notebook = gtk_notebook_new();
    const char* const kTabs[] = {"Files", "Agent", "CTXMon"};
    for (const char* tab : kTabs) {
        GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start(page, 6);
        gtk_widget_set_margin_end(page, 6);
        gtk_widget_set_margin_top(page, 6);
        gtk_widget_set_margin_bottom(page, 6);
        std::string title = std::string(tab) + " panel (sample)";
        gtk_box_append(GTK_BOX(page), gtk_label_new(title.c_str()));
        GtkWidget* entry = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(entry), "Sample entry");
        gtk_box_append(GTK_BOX(page), entry);
        GtkWidget* button = gtk_button_new_with_label("Sample button");
        gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "app.noop");
        gtk_box_append(GTK_BOX(page), button);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(tab));
    }
    return notebook;
}

GtkWidget* build_status_bar(GtkLabel** message) {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(bar, "vm-statusbar");
    GtkWidget* label = gtk_label_new("Starting");
    gtk_widget_add_css_class(label, "vm-status-message");
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(bar), label);
    *message = GTK_LABEL(label);
    const char* const kButtons[] = {"C++", "Tab Width: 4", "UTF-8", "LF"};
    for (const char* text : kButtons) {
        GtkWidget* button = gtk_button_new_with_label(text);
        gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
        gtk_widget_add_css_class(button, "vm-status-panel");
        gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "app.noop");
        gtk_box_append(GTK_BOX(bar), button);
    }
    const char* const kLabels[] = {"Ln 1, Col 1", "INS"};
    for (const char* text : kLabels) {
        GtkWidget* panel = gtk_label_new(text);
        gtk_widget_add_css_class(panel, "vm-status-panel");
        gtk_box_append(GTK_BOX(bar), panel);
    }
    return bar;
}

} // namespace vermilion::spike
