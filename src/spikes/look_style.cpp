// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: reads the dark-mode signals, picks the palette and loads the stylesheet.

#include "spikes/look_style.h"

#include <cctype>
#include <cstdio>

#include "ui/gtkx/string.h"

namespace vermilion::spike {

namespace {

using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

struct FollowAnswer {
    bool dark;
    const char* reason;
};

const char* mode_name(Mode mode) {
    switch (mode) {
    case Mode::Light:
        return "light";
    case Mode::Dark:
        return "dark";
    case Mode::Follow:
        return "follow";
    }
    return "follow";
}

bool ends_with_dark(const std::string& name) {
    const std::string suffix = "-dark";
    if (name.size() < suffix.size()) {
        return false;
    }
    std::size_t offset = name.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(name[offset + i])));
        if (c != suffix[i]) {
            return false;
        }
    }
    return true;
}

// The candidate rule for "Follow system": portal first, then the theme name, then prefer-dark.
FollowAnswer follow_answer(const LookState& s) {
    if (s.portal_scheme == PortalScheme::Dark) {
        return {true, "portal"};
    }
    if (s.portal_scheme == PortalScheme::Light) {
        return {false, "portal"};
    }
    if (ends_with_dark(s.desktop_theme)) {
        return {true, "theme-name"};
    }
    if (s.desktop_prefer_dark) {
        return {true, "prefer-dark"};
    }
    return {false, "none"};
}

bool append_file(const std::string& path, std::string& out) {
    char* contents = nullptr;
    gsize length = 0;
    GError* raw_error = nullptr;
    if (!g_file_get_contents(path.c_str(), &contents, &length, &raw_error)) {
        ErrorPtr error(raw_error, &g_error_free);
        g_printerr("LOOK css-read-failed %s: %s\n", path.c_str(),
                   error ? error->message : "unknown error");
        return false;
    }
    out += vermilion::gtkx::take_string(contents);
    out += "\n";
    return true;
}

// Palette first (its @define-color lines), then the shared structure.
void load_css(LookState& s, bool dark) {
    std::string palette = vermilion::gtkx::take_string(
        g_build_filename(s.data_dir.c_str(), dark ? "look_dark.css" : "look_light.css", nullptr));
    std::string structure = vermilion::gtkx::take_string(
        g_build_filename(s.data_dir.c_str(), "look_structure.css", nullptr));
    std::string text;
    if (!append_file(palette, text) || !append_file(structure, text)) {
        return;
    }
    gtk_css_provider_load_from_string(s.provider.get(), text.c_str());
}

// Returns the id of the scheme applied to every buffer, or "none".
const char* apply_scheme(LookState& s, bool dark) {
    GtkSourceStyleSchemeManager* manager = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme* scheme =
        gtk_source_style_scheme_manager_get_scheme(manager, dark ? "classic-dark" : "classic");
    if (scheme == nullptr && dark) {
        scheme = gtk_source_style_scheme_manager_get_scheme(manager, "oblivion");
    }
    for (GtkSourceBuffer* buffer : s.buffers) {
        gtk_source_buffer_set_style_scheme(buffer, scheme);
    }
    return scheme != nullptr ? gtk_source_style_scheme_get_id(scheme) : "none";
}

void report(LookState& s, const char* why, const FollowAnswer& follow, bool dark,
            const char* scheme_id) {
    std::string now = read_theme_name(gtk_settings_get_default());
    std::string line = std::string("mode=") + mode_name(s.mode) +
                       " base=" + (s.base_default ? "default" : "desktop") + " gtk_theme=" + now +
                       " desktop_theme=" + s.desktop_theme + (s.base_default ? "(stale)" : "") +
                       " prefer_dark=" + (s.desktop_prefer_dark ? "1" : "0") +
                       (s.prefer_dark_pinned ? "(pinned)" : "") +
                       " portal=" + vermilion::spike::portal_scheme_name(s.portal_scheme) +
                       " follow=" + (follow.dark ? "dark" : "light") + "(" + follow.reason + ")" +
                       " effective=" + (dark ? "dark" : "light") + " scheme=" + scheme_id;
    g_print("LOOK %s %s\n", why, line.c_str());
    fflush(stdout);
    if (s.message != nullptr) {
        gtk_label_set_text(s.message, line.c_str());
    }
}

} // namespace

std::string read_theme_name(GtkSettings* settings) {
    char* name = nullptr;
    g_object_get(settings, "gtk-theme-name", &name, nullptr);
    return vermilion::gtkx::take_string(name);
}

bool read_prefer_dark(GtkSettings* settings) {
    gboolean value = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &value, nullptr);
    return value != FALSE;
}

// Touching a setting the spike has not changed makes GTK announce it again, which re-enters
// apply(), so each setting is set or reset only when its pinned state changes.
void apply(LookState& s, const char* why) {
    GtkSettings* settings = gtk_settings_get_default();
    s.applying = true;
    if (s.base_default && !s.theme_pinned) {
        g_object_set(settings, "gtk-theme-name", "Default", nullptr);
        s.theme_pinned = true;
    } else if (!s.base_default && s.theme_pinned) {
        gtk_settings_reset_property(settings, "gtk-theme-name");
        s.theme_pinned = false;
    }
    s.applying = false;
    if (!s.theme_pinned) {
        s.desktop_theme = read_theme_name(settings);
    }
    if (!s.prefer_dark_pinned) {
        s.desktop_prefer_dark = read_prefer_dark(settings);
    }
    FollowAnswer follow = follow_answer(s);
    bool dark = s.mode == Mode::Dark || (s.mode == Mode::Follow && follow.dark);
    bool want_pin = dark && !s.desktop_prefer_dark;
    s.applying = true;
    if (want_pin && !s.prefer_dark_pinned) {
        g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, nullptr);
        s.prefer_dark_pinned = true;
    } else if (!want_pin && s.prefer_dark_pinned) {
        gtk_settings_reset_property(settings, "gtk-application-prefer-dark-theme");
        s.prefer_dark_pinned = false;
    }
    s.applying = false;
    load_css(s, dark);
    const char* scheme_id = apply_scheme(s, dark);
    report(s, why, follow, dark, scheme_id);
}

} // namespace vermilion::spike
