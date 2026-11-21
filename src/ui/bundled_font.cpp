// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The bundled font: writes the files from the resources and registers them with fontconfig.

#include "ui/bundled_font.h"

#include <fontconfig/fontconfig.h>
#include <gio/gio.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

#include <memory>

#include "core/cache_file.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

const std::vector<std::string>& bundled_font_files() {
    static const std::vector<std::string> files = {
        "FantasqueSansMono-Regular.ttf", "FantasqueSansMono-Bold.ttf",
        "FantasqueSansMono-Italic.ttf", "FantasqueSansMono-BoldItalic.ttf"};
    return files;
}

std::string bundled_font_folder() {
    return gtkx::take_string(
        g_build_filename(g_get_user_cache_dir(), "vermilion", "fonts", nullptr));
}

std::vector<std::string> write_bundled_font(const std::string& folder) {
    std::vector<std::string> paths;
    for (const std::string& name : bundled_font_files()) {
        const std::string resource = std::string(kBundledFontResourceDir) + "/" + name;
        std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
            g_resources_lookup_data(resource.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
            &g_bytes_unref);
        if (!bytes) {
            continue;
        }
        gsize size = 0;
        const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
        if (write_if_changed(folder, name, std::string(data, size)) == CacheWrite::Failed) {
            continue;
        }
        paths.push_back(gtkx::take_string(g_build_filename(folder.c_str(), name.c_str(), nullptr)));
    }
    return paths;
}

int register_font_files(const std::vector<std::string>& paths) {
    int accepted = 0;
    for (const std::string& path : paths) {
        if (FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8*>(path.c_str()))) {
            ++accepted;
        }
    }
    if (accepted > 0) {
        PangoFontMap* map = pango_cairo_font_map_get_default();
        if (PANGO_IS_FC_FONT_MAP(map)) {
            pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(map));
        }
    }
    return accepted;
}

int install_bundled_font() {
    static const int registered = [] {
        const std::string folder = bundled_font_folder();
        const int count = register_font_files(write_bundled_font(folder));
        if (count < 4) {
            g_warning("Vermilion: only %d of 4 files of the bundled font %s could be registered "
                      "(folder %s); the system monospace font is used.",
                      count, kBundledFontFamily, folder.c_str());
        }
        return count;
    }();
    return registered;
}

} // namespace vermilion::ui
