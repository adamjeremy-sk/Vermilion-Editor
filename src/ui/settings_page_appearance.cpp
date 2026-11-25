// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Appearance tab: builds its controls and maps them to the settings.

#include "ui/settings_page_appearance.h"

#include <pango/pango.h>

#include "ui/gtkx/object_ptr.h"
#include "ui/settings_page_layout.h"

namespace vermilion::ui {
namespace {

// The text of `raw` (an empty string for null), then `g_free(raw)`. Used for the result of
// pango_font_description_to_string, which the caller owns.
std::string take_text(char* raw) {
    std::string text = raw == nullptr ? std::string() : std::string(raw);
    g_free(raw);
    return text;
}

// The function of a GtkCustomFilter (GtkCustomFilterFunc): keep the item only when it is monospace.
gboolean accept_font_item(gpointer item, gpointer) {
    return AppearancePage::is_monospace_item(item) ? TRUE : FALSE;
}

} // namespace

unsigned AppearancePage::colour_mode_index(ColourMode mode) {
    switch (mode) {
    case ColourMode::Light:
        return 0;
    case ColourMode::Dark:
        return 1;
    case ColourMode::System:
        return 2;
    }
    return 2;
}

ColourMode AppearancePage::colour_mode_at(unsigned index) {
    switch (index) {
    case 0:
        return ColourMode::Light;
    case 1:
        return ColourMode::Dark;
    default:
        return ColourMode::System;
    }
}

bool AppearancePage::is_monospace_item(gpointer item) {
    if (item == nullptr) {
        return false;
    }
    if (PANGO_IS_FONT_FAMILY(item)) {
        return pango_font_family_is_monospace(PANGO_FONT_FAMILY(item)) != FALSE;
    }
    if (PANGO_IS_FONT_FACE(item)) {
        PangoFontFamily* family =
            pango_font_face_get_family(PANGO_FONT_FACE(item)); // borrowed; may be null
        return family != nullptr && pango_font_family_is_monospace(family) != FALSE;
    }
    return false;
}

const char* AppearancePage::default_font_text() {
    return settings::kFontFirstChoice;
}

AppearancePage::AppearancePage() {
    column_ = settings_column(); // floating; the notebook takes it over

    const char* const modes[] = {"Light", "Dark", "Follow system", nullptr};
    colour_ = gtk_drop_down_new_from_strings(modes); // floating; the row takes it over
    gtk_box_append(GTK_BOX(column_), settings_row("Colour _mode:", colour_));
    highlight_ = gtk_check_button_new_with_mnemonic("_Highlight the current line"); // floating
    gtk_box_append(GTK_BOX(column_), highlight_);
    system_font_ = gtk_check_button_new_with_mnemonic("Use the _system monospace font"); // floating
    gtk_box_append(GTK_BOX(column_), system_font_);
    font_button_ = build_font_button(); // floating; the row takes it over
    gtk_box_append(GTK_BOX(column_), settings_row("_Font:", font_button_));

    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        colour_, "notify::selected", [this](GObject*, GParamSpec*) { notify_changed(); }));
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        highlight_, "toggled", [this](GtkCheckButton*) { notify_changed(); }));
    signals_.push_back(
        gtkx::connect<void(GtkCheckButton*)>(system_font_, "toggled", [this](GtkCheckButton*) {
            update_font_sensitivity();
            notify_changed();
        }));
    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        font_button_, "notify::font-desc", [this](GObject*, GParamSpec*) { notify_changed(); }));
}

GtkWidget* AppearancePage::build_font_button() {
    // gtk_font_dialog_new and gtk_custom_filter_new return full references; the two ObjectPtrs own
    // them. The dialog takes its own reference to the filter, and the button its own to the dialog,
    // so ours go when the two ObjectPtrs end.
    auto dialog = gtkx::ObjectPtr<GtkFontDialog>::adopt(gtk_font_dialog_new());
    gtk_font_dialog_set_title(dialog.get(), "Editor font");
    auto filter = gtkx::ObjectPtr<GtkCustomFilter>::adopt(
        gtk_custom_filter_new(&accept_font_item, nullptr, nullptr));
    gtk_font_dialog_set_filter(dialog.get(), GTK_FILTER(filter.get()));
    GtkWidget* button = gtk_font_dialog_button_new(nullptr); // floating; the row takes it over
    gtk_font_dialog_button_set_dialog(GTK_FONT_DIALOG_BUTTON(button), dialog.get());
    return button;
}

void AppearancePage::show_font(const std::string& text) {
    PangoFontDescription* description = pango_font_description_from_string(text.c_str());
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(font_button_),
                                         description); // the button copies it
    pango_font_description_free(description);
}

std::string AppearancePage::font_text() const {
    const PangoFontDescription* description =
        gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(font_button_)); // borrowed
    if (description == nullptr) {
        return settings::kFontFirstChoice;
    }
    return take_text(pango_font_description_to_string(description));
}

void AppearancePage::update_font_sensitivity() {
    const bool system = gtk_check_button_get_active(GTK_CHECK_BUTTON(system_font_)) != FALSE;
    gtk_widget_set_sensitive(font_button_, system ? FALSE : TRUE);
}

void AppearancePage::show_settings(const EditorSettings& settings) {
    gtk_drop_down_set_selected(GTK_DROP_DOWN(colour_), colour_mode_index(settings.colour_mode));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(highlight_), settings.highlight_current_line);
    const bool system = settings.font.empty();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(system_font_), system);
    shown_text_ = system ? std::string() : settings.font;
    show_font(system ? std::string(settings::kFontFirstChoice) : settings.font);
    shown_canonical_ = system ? std::string() : font_text();
    update_font_sensitivity();
}

void AppearancePage::store(EditorSettings& settings) const {
    settings.colour_mode = colour_mode_at(gtk_drop_down_get_selected(GTK_DROP_DOWN(colour_)));
    settings.highlight_current_line =
        gtk_check_button_get_active(GTK_CHECK_BUTTON(highlight_)) != FALSE;
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(system_font_)) != FALSE) {
        settings.font.clear();
        return;
    }
    const std::string current = font_text();
    settings.font = (!shown_text_.empty() && current == shown_canonical_) ? shown_text_ : current;
}

} // namespace vermilion::ui
