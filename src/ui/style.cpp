// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Loads a palette and the structure stylesheet from the resources into one CSS provider.

#include "ui/style.h"

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <utility>

#include "core/log.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr const char* kLightPalette = "/io/github/adamjeremy_sk/Vermilion/classic_light.css";
constexpr const char* kDarkPalette = "/io/github/adamjeremy_sk/Vermilion/classic_dark.css";
constexpr const char* kStructure = "/io/github/adamjeremy_sk/Vermilion/classic_structure.css";
constexpr const char* kPreferDark = "gtk-application-prefer-dark-theme";

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;

// Appends the resource's text and a newline to out; logs and returns false if it is missing.
bool append_resource(const char* path, std::string& out) {
    BytesPtr bytes(g_resources_lookup_data(path, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                   &g_bytes_unref);
    if (!bytes) {
        logging::error(std::string("stylesheet resource missing: ") + path);
        return false;
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    out.append(data, size);
    out.push_back('\n');
    return true;
}

// The palette first, then the structure: the same order as before.
bool build_css(bool dark, std::string& css) {
    css.clear();
    return append_resource(dark ? kDarkPalette : kLightPalette, css) &&
           append_resource(kStructure, css);
}

// The value of gtk-application-prefer-dark-theme as GTK has it now.
bool prefer_dark_flag(GtkSettings* settings) {
    gboolean value = FALSE;
    g_object_get(settings, kPreferDark, &value, nullptr);
    return value != FALSE;
}

// Sets the flag to `dark` only when it differs; setting an unchanged value would make GTK announce
// it again, and a listener that reacts to the announcement then loops.
void match_flag(bool dark) {
    GtkSettings* settings = gtk_settings_get_default(); // borrowed; null without a display
    if (settings == nullptr || prefer_dark_flag(settings) == dark) {
        return;
    }
    g_object_set(settings, kPreferDark, dark ? TRUE : FALSE, nullptr);
}

} // namespace

std::unique_ptr<Style> Style::install(bool dark) {
    GdkDisplay* display = gdk_display_get_default(); // borrowed
    if (display == nullptr) {
        logging::error("no display for the stylesheet");
        return nullptr;
    }
    std::string css;
    if (!build_css(dark, css)) {
        return nullptr;
    }
    // Full reference from _new (not floating); the display keeps its own once it is added.
    auto provider = gtkx::ObjectPtr<GtkCssProvider>::adopt(gtk_css_provider_new());
    gtkx::SignalConnection errors = gtkx::connect<void(GtkCssProvider*, GtkCssSection*, GError*)>(
        provider.get(), "parsing-error",
        [](GtkCssProvider*, GtkCssSection* section, GError* error) {
            std::string where = gtkx::take_string(gtk_css_section_to_string(section));
            logging::warning("stylesheet " + where + ": " +
                             (error != nullptr ? error->message : "unknown error"));
        });
    gtk_css_provider_load_from_string(provider.get(), css.c_str());
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider.get()),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    GtkSettings* settings = gtk_settings_get_default(); // borrowed
    const bool at_start = settings != nullptr && prefer_dark_flag(settings);
    match_flag(dark);
    return std::make_unique<Style>(Token{}, std::move(provider), std::move(errors), dark, at_start);
}

Style::Style(Token, gtkx::ObjectPtr<GtkCssProvider> provider, gtkx::SignalConnection parsing_errors,
             bool dark, bool prefer_dark_at_start)
    : provider_(std::move(provider)), parsing_errors_(std::move(parsing_errors)), dark_(dark),
      prefer_dark_at_start_(prefer_dark_at_start) {}

Style::~Style() {
    GdkDisplay* display = gdk_display_get_default(); // borrowed
    if (display != nullptr) {
        gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(provider_.get()));
    }
}

bool Style::set_dark(bool dark) {
    if (dark == dark_) {
        return true;
    }
    std::string css;
    if (!build_css(dark, css)) {
        return false;
    }
    gtk_css_provider_load_from_string(provider_.get(), css.c_str());
    match_flag(dark);
    dark_ = dark;
    return true;
}

} // namespace vermilion::ui
