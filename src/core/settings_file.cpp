// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// SettingsFile: settings.ini read and written through GKeyFile.

#include "core/settings_file.h"

#include <glib/gstdio.h>

#include <cerrno>
#include <string>
#include <utility>

namespace vermilion {

namespace {

using KeyFilePtr = std::unique_ptr<GKeyFile, decltype(&g_key_file_free)>;

KeyFilePtr new_key_file() {
    return KeyFilePtr(g_key_file_new(), &g_key_file_free);
}

SettingsLoad read_into(GKeyFile* keys, const std::string& path) {
    GError* raw = nullptr;
    if (g_key_file_load_from_file(keys, path.c_str(), G_KEY_FILE_KEEP_COMMENTS, &raw))
        return SettingsLoad::Loaded;
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (g_error_matches(error.get(), G_FILE_ERROR, G_FILE_ERROR_NOENT))
        return SettingsLoad::Missing;
    if (error->domain == G_KEY_FILE_ERROR)
        return SettingsLoad::Damaged;
    return SettingsLoad::Unreadable;
}

} // namespace

std::string settings_unusable_message(const std::string& path) {
    return "Could not read " + path + "; using the default settings.";
}

SettingsFile::SettingsFile(std::string path) : path_(std::move(path)), keys_(new_key_file()) {}

std::string SettingsFile::default_path() {
    g_autofree char* built = g_build_filename(g_get_user_config_dir(), "vermilion", "settings.ini",
                                              static_cast<char*>(nullptr));
    return built;
}

SettingsLoad SettingsFile::load() {
    changes_.clear();
    KeyFilePtr fresh = new_key_file();
    const SettingsLoad result = read_into(fresh.get(), path_);
    state_ = result;
    if (result == SettingsLoad::Loaded)
        keys_ = std::move(fresh);
    else
        keys_ = new_key_file();
    return state_;
}

bool SettingsFile::get_bool(const char* group, const char* key, bool fallback) const {
    GError* raw = nullptr;
    const gboolean value = g_key_file_get_boolean(keys_.get(), group, key, &raw);
    if (raw != nullptr) {
        g_error_free(raw);
        return fallback;
    }
    return value != FALSE;
}

void SettingsFile::set_bool(const char* group, const char* key, bool value) {
    const char* text = value ? "true" : "false";
    g_key_file_set_value(keys_.get(), group, key, text);
    remember(group, key, text);
}

int SettingsFile::get_int(const char* group, const char* key, int fallback) const {
    GError* raw = nullptr;
    const gint value = g_key_file_get_integer(keys_.get(), group, key, &raw);
    if (raw != nullptr) {
        g_error_free(raw);
        return fallback;
    }
    return value;
}

void SettingsFile::set_int(const char* group, const char* key, int value) {
    const std::string text = std::to_string(value);
    g_key_file_set_value(keys_.get(), group, key, text.c_str());
    remember(group, key, text.c_str());
}

std::string SettingsFile::get_string(const char* group, const char* key,
                                     const std::string& fallback) const {
    GError* raw = nullptr;
    g_autofree char* value = g_key_file_get_string(keys_.get(), group, key, &raw);
    if (raw != nullptr) {
        g_error_free(raw);
        return fallback;
    }
    return std::string(value);
}

void SettingsFile::set_string(const char* group, const char* key, const std::string& value) {
    g_key_file_set_string(keys_.get(), group, key, value.c_str());
    g_autofree char* escaped = g_key_file_get_value(keys_.get(), group, key, nullptr);
    remember(group, key, escaped != nullptr ? escaped : "");
}

void SettingsFile::remember(const char* group, const char* key, const char* text) {
    for (Change& change : changes_) {
        if (change.group == group && change.key == key) {
            change.value = text;
            return;
        }
    }
    changes_.push_back(Change{std::string(group), std::string(key), text});
}

Status SettingsFile::save() {
    if (changes_.empty())
        return ok_status();

    KeyFilePtr merged = new_key_file();
    const SettingsLoad on_disk = read_into(merged.get(), path_);
    if (on_disk == SettingsLoad::Unreadable)
        return Status::err(Error{"Could not read " + path_ + " before saving the settings"});

    g_autofree char* folder = g_path_get_dirname(path_.c_str());
    if (g_mkdir_with_parents(folder, 0700) != 0) {
        const char* reason = g_strerror(errno);
        return Status::err(Error{"Could not create " + std::string(folder) + ": " + reason});
    }

    if (on_disk == SettingsLoad::Damaged) {
        if (g_rename(path_.c_str(), (path_ + ".bad").c_str()) != 0) {
            const char* reason = g_strerror(errno);
            return Status::err(Error{"Could not move " + path_ + " aside: " + reason});
        }
        merged = new_key_file();
    }

    for (const Change& change : changes_)
        g_key_file_set_value(merged.get(), change.group.c_str(), change.key.c_str(),
                             change.value.c_str());

    gsize length = 0;
    g_autofree char* text = g_key_file_to_data(merged.get(), &length, nullptr);
    GError* raw = nullptr;
    if (!g_file_set_contents(path_.c_str(), text, static_cast<gssize>(length), &raw)) {
        std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
        return Status::err(Error{"Could not write " + path_ + ": " + error->message});
    }

    keys_ = std::move(merged);
    changes_.clear();
    state_ = SettingsLoad::Loaded;
    return ok_status();
}

} // namespace vermilion
