// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The user's settings file (settings.ini): typed reads and writes that keep the keys it does not
// know.

#pragma once

#include <glib.h>

#include <memory>
#include <string>
#include <vector>

#include "core/result.h"

namespace vermilion {

// What load() found.
enum class SettingsLoad {
    Loaded,     // the file was read (an empty file counts)
    Missing,    // there is no file: the defaults apply and nothing is created until save()
    Damaged,    // the file is not valid key-file text: the defaults apply and the file is untouched
    Unreadable, // the file exists but cannot be read (permissions, I/O): as Damaged, never moved
};

// The text for the status line when load() returned Damaged or Unreadable.
std::string settings_unusable_message(const std::string& path);

// One settings.ini through GKeyFile ([group] key=value lines; comments and unknown groups and
// keys are kept). Reads fall back to the caller's default for a missing key or a value of the wrong
// type. Changes are made in memory (set_bool, set_int, set_string) and written by save(), which
// reads the file again first and applies only the keys changed here, so two programs (the light
// editor and the IDE) can write the same file without undoing each other's keys. save() writes
// through a temporary file and a rename in the same folder (g_file_set_contents), so a reader never
// sees half a file and a failed save leaves nothing behind. Main thread only; nothing here touches
// GTK.
class SettingsFile {
public:
    // `path` is the full path of the file; the folder is created by save() when it is missing.
    explicit SettingsFile(std::string path);

    // $XDG_CONFIG_HOME/vermilion/settings.ini (g_get_user_config_dir()).
    static std::string default_path();

    // Reads the file into memory, replacing anything held (pending changes included). A file that
    // is Missing, Damaged or Unreadable leaves an empty set of settings, so every read returns its
    // default.
    SettingsLoad load();

    // The result of the last load() or save(); Missing before the first load().
    SettingsLoad state() const { return state_; }

    // The boolean at group/key, or `fallback` when the key is missing or is not a boolean
    // (GKeyFile accepts "true", "false", "1" and "0").
    bool get_bool(const char* group, const char* key, bool fallback) const;

    // Sets a boolean in memory (visible to get_bool at once) and remembers it for save().
    void set_bool(const char* group, const char* key, bool value);

    // The integer at group/key, or `fallback` when the key is missing or its value is not a whole
    // number that fits an int (for example "abc", "1.5", "" or "99999999999").
    int get_int(const char* group, const char* key, int fallback) const;

    // As set_bool, for an integer (written in decimal).
    void set_int(const char* group, const char* key, int value);

    // The text at group/key with the key-file escapes decoded (backslash-n is a line feed, and so
    // on), or `fallback` when the key is missing or the value has an invalid escape. An empty value
    // is returned as the empty string, not as the fallback.
    std::string get_string(const char* group, const char* key, const std::string& fallback) const;

    // As set_bool, for text. Control characters and backslashes are escaped when the text is
    // written, so get_string returns exactly `value` after a save() and a load().
    void set_string(const char* group, const char* key, const std::string& value);

    // With nothing remembered (no set_ call since the last load() or save()) it does nothing and
    // returns ok: the file is neither created nor touched. Otherwise it writes the file: reads it
    // again from disk, applies the remembered changes, writes the result through a temporary file
    // and a rename, and keeps the written text as the current settings. If the file on disk is
    // Damaged (not valid key-file text) it is first renamed to <path>.bad (an older .bad is
    // replaced) and a new file holds only the remembered changes. If it is Unreadable, nothing is
    // written and the error says so. An error is also returned when the folder cannot be created or
    // the file cannot be written; the remembered changes are kept so that save() can be tried
    // again.
    Status save();

private:
    // Remembers the text written after the "=" for save(): replaces an earlier change of the same
    // group and key, otherwise appends one. It does not touch the in-memory key file.
    void remember(const char* group, const char* key, const char* text);

    struct Change {
        std::string group;
        std::string key;
        std::string value; // the text written after the "="
    };

    std::string path_;
    std::unique_ptr<GKeyFile, decltype(&g_key_file_free)> keys_;
    std::vector<Change> changes_;
    SettingsLoad state_ = SettingsLoad::Missing;
};

} // namespace vermilion
