// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Writes the editor's style scheme file from the program's resources and finds the scheme.

#include "ui/editor_scheme.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtksourceview/gtksource.h>

#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr char kSchemeResource[] =
    "/io/github/adamjeremy_sk/Vermilion/styles/vermilion-classic.xml";
constexpr char kSchemeFile[] = "vermilion-classic.xml";
constexpr char kSchemeId[] = "vermilion-classic";
constexpr char kParentSchemeId[] = "classic";

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

std::string scheme_folder() {
    return gtkx::take_string(
        g_build_filename(g_get_user_cache_dir(), "vermilion", "styles", nullptr));
}

bool write_scheme_file(const std::string& folder) {
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_resources_lookup_data(kSchemeResource, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
        &g_bytes_unref);
    if (!bytes) {
        return false;
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    const std::string wanted(data, size);
    if (g_mkdir_with_parents(folder.c_str(), 0700) != 0) {
        return false;
    }
    const std::string path =
        gtkx::take_string(g_build_filename(folder.c_str(), kSchemeFile, nullptr));
    if (read_file(path) == wanted) {
        return true;
    }
    GError* raw = nullptr;
    const gboolean written =
        g_file_set_contents(path.c_str(), wanted.data(), static_cast<gssize>(wanted.size()), &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    return written != FALSE;
}

GtkSourceStyleScheme* editor_scheme() {
    GtkSourceStyleSchemeManager* manager = gtk_source_style_scheme_manager_get_default();
    static const bool ready = [manager] {
        const std::string folder = scheme_folder();
        if (!write_scheme_file(folder)) {
            g_warning("Vermilion: could not write the editor style scheme to %s; using the "
                      "classic scheme.",
                      folder.c_str());
            return false;
        }
        gtk_source_style_scheme_manager_append_search_path(manager, folder.c_str());
        return true;
    }();
    (void)ready;
    GtkSourceStyleScheme* scheme = gtk_source_style_scheme_manager_get_scheme(manager, kSchemeId);
    if (scheme == nullptr) {
        scheme = gtk_source_style_scheme_manager_get_scheme(manager, kParentSchemeId);
    }
    return scheme;
}

} // namespace vermilion::ui
