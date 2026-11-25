// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Appearance tab of the Settings window: colour mode, current line, editor font.

#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "core/colour_mode.h"
#include "core/editor_settings.h"
#include "ui/gtkx/signal.h"
#include "ui/settings_page.h"

namespace vermilion::ui {

// The fields colour_mode, highlight_current_line and font of EditorSettings. Controls (design of
// M2.5): a drop-down with Light, Dark and Follow system; a check box "Highlight the current line";
// a check box "Use the system monospace font" and a font button, which is disabled while that box
// is checked. The font dialog lists monospace fonts only.
class AppearancePage : public SettingsPage {
public:
    AppearancePage();

    GtkWidget* widget() const override { return column_; }
    void store(EditorSettings& settings) const override;

    // Where each colour mode stands in the drop-down: Light 0, Dark 1, Follow system 2.
    static unsigned colour_mode_index(ColourMode mode);
    // The inverse; an index outside 0 to 2 gives System.
    static ColourMode colour_mode_at(unsigned index);

    // True for a PangoFontFamily that is monospace, and for a PangoFontFace whose family is; false
    // for anything else, a null pointer included. The font dialog's filter calls it.
    static bool is_monospace_item(gpointer item);

    // The font shown (greyed out) while the setting holds no font of its own, and the one the
    // button switches to first when the system font box is unchecked.
    static const char* default_font_text();

    // The controls (borrowed), for the tests.
    GtkWidget* colour_dropdown() const { return colour_; }
    GtkWidget* highlight_check() const { return highlight_; }
    GtkWidget* system_font_check() const { return system_font_; }
    GtkWidget* font_button() const { return font_button_; }

protected:
    void show_settings(const EditorSettings& settings) override;

private:
    GtkWidget* build_font_button();
    // Makes the font button show `text` (a Pango font description, never empty).
    void show_font(const std::string& text);
    // The font button's description as text, in Pango's own form.
    std::string font_text() const;
    // Enables the font button exactly when the system font box is not checked.
    void update_font_sensitivity();

    GtkWidget* column_ = nullptr; // the widgets are borrowed; the page's widget owns them
    GtkWidget* colour_ = nullptr;
    GtkWidget* highlight_ = nullptr;
    GtkWidget* system_font_ = nullptr;
    GtkWidget* font_button_ = nullptr;
    // The font text of the last show_settings() and what the button made of it, so that a font the
    // person did not touch is stored exactly as it was read, not in Pango's spelling of it.
    std::string shown_text_;
    std::string shown_canonical_;
    std::vector<gtkx::SignalConnection> signals_;
};

} // namespace vermilion::ui
