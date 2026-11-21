// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Writes the editor's style scheme files from the program's resources and finds the schemes.

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

constexpr char kLightResource[] = "/io/github/adamjeremy_sk/Vermilion/styles/vermilion-classic.xml";
constexpr char kDarkResource[] =
    "/io/github/adamjeremy_sk/Vermilion/styles/vermilion-classic-dark.xml";
constexpr char kLightFile[] = "vermilion-classic.xml";
constexpr char kDarkFile[] = "vermilion-classic-dark.xml";
constexpr char kLightId[] = "vermilion-classic";
constexpr char kDarkId[] = "vermilion-classic-dark";
constexpr char kLightParentId[] = "classic";
constexpr char kDarkParentId[] = "classic-dark";

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool write_one(const char* resource, const std::string& folder, const char* file) {
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_resources_lookup_data(resource, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr), &g_bytes_unref);
    if (!bytes) {
        return false;
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    const std::string wanted(data, size);
    if (g_mkdir_with_parents(folder.c_str(), 0700) != 0) {
        return false;
    }
    const std::string path = gtkx::take_string(g_build_filename(folder.c_str(), file, nullptr));
    if (read_file(path) == wanted) {
        return true;
    }
    GError* raw = nullptr;
    const gboolean written =
        g_file_set_contents(path.c_str(), wanted.data(), static_cast<gssize>(wanted.size()), &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    return written != FALSE;
}

} // namespace

std::string scheme_folder() {
    return gtkx::take_string(
        g_build_filename(g_get_user_cache_dir(), "vermilion", "styles", nullptr));
}

bool write_scheme_files(const std::string& folder) {
    const bool light = write_one(kLightResource, folder, kLightFile);
    const bool dark = write_one(kDarkResource, folder, kDarkFile);
    return light && dark;
}

GtkSourceStyleScheme* editor_scheme(bool dark) {
    GtkSourceStyleSchemeManager* manager = gtk_source_style_scheme_manager_get_default();
    static const bool ready = [manager] {
        const std::string folder = scheme_folder();
        if (!write_scheme_files(folder)) {
            g_warning("Vermilion: could not write the editor style schemes to %s; using the "
                      "classic schemes.",
                      folder.c_str());
            return false;
        }
        gtk_source_style_scheme_manager_append_search_path(manager, folder.c_str());
        return true;
    }();
    (void)ready;
    GtkSourceStyleScheme* scheme =
        gtk_source_style_scheme_manager_get_scheme(manager, dark ? kDarkId : kLightId);
    if (scheme == nullptr) {
        scheme = gtk_source_style_scheme_manager_get_scheme(manager,
                                                            dark ? kDarkParentId : kLightParentId);
    }
    return scheme;
}

} // namespace vermilion::ui
