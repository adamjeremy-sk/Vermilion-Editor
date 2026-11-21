// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// FontStyle: one CSS provider on the display that holds the editor font rule.

#include "ui/font_style.h"

#include <memory>
#include <string>
#include <utility>

#include "core/log.h"
#include "ui/font_css.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

std::unique_ptr<FontStyle> FontStyle::install() {
    GdkDisplay* display = gdk_display_get_default(); // borrowed
    if (display == nullptr) {
        logging::error("no display for the editor font");
        return nullptr;
    }
    // Full reference from _new (not floating); the display keeps its own once it is added.
    auto provider = gtkx::ObjectPtr<GtkCssProvider>::adopt(gtk_css_provider_new());
    gtkx::SignalConnection errors = gtkx::connect<void(GtkCssProvider*, GtkCssSection*, GError*)>(
        provider.get(), "parsing-error",
        [](GtkCssProvider*, GtkCssSection* section, GError* error) {
            std::string where = gtkx::take_string(gtk_css_section_to_string(section));
            logging::warning("font rule " + where + ": " +
                             (error != nullptr ? error->message : "unknown error"));
        });
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider.get()),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return std::make_unique<FontStyle>(Token{}, std::move(provider), std::move(errors));
}

FontStyle::FontStyle(Token, gtkx::ObjectPtr<GtkCssProvider> provider,
                     gtkx::SignalConnection parsing_errors)
    : provider_(std::move(provider)), parsing_errors_(std::move(parsing_errors)) {}

FontStyle::~FontStyle() {
    GdkDisplay* display = gdk_display_get_default(); // borrowed
    if (display != nullptr) {
        gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(provider_.get()));
    }
}

void FontStyle::set_font(const std::string& description) {
    if (description == font_) {
        return;
    }
    gtk_css_provider_load_from_string(provider_.get(), font_css(description).c_str());
    font_ = description;
}

} // namespace vermilion::ui
