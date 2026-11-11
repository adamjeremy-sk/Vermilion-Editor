// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Loads the palette and structure stylesheets from the resources into one CSS provider.

#include "ui/style.h"

#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "core/log.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr const char* kPalette = "/io/github/adamjeremy_sk/Vermilion/classic_light.css";
constexpr const char* kStructure = "/io/github/adamjeremy_sk/Vermilion/classic_structure.css";

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

} // namespace

void install_classic_style() {
    std::string css;
    if (!append_resource(kPalette, css) || !append_resource(kStructure, css)) {
        return;
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
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider.get()),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

} // namespace vermilion::ui
