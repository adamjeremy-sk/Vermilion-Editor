// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Writes a file into a cache folder only when its content differs.

#include "core/cache_file.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>

namespace vermilion {

namespace {

bool plain_file_name(const std::string& name) {
    return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos;
}

bool holds(const std::string& path, const std::string& bytes) {
    GStatBuf info;
    if (g_lstat(path.c_str(), &info) != 0) {
        return false;
    }
    if (!S_ISREG(info.st_mode) || static_cast<gsize>(info.st_size) != bytes.size()) {
        return false;
    }
    g_autofree gchar* raw = nullptr;
    gsize length = 0;
    g_autoptr(GError) error = nullptr;
    if (!g_file_get_contents(path.c_str(), &raw, &length, &error)) {
        return false;
    }
    return length == bytes.size() && (length == 0 || bytes.compare(0, length, raw, length) == 0);
}

} // namespace

CacheWrite write_if_changed(const std::string& folder, const std::string& name,
                            const std::string& bytes) {
    if (!plain_file_name(name)) {
        return CacheWrite::Failed;
    }
    if (g_mkdir_with_parents(folder.c_str(), 0700) != 0) {
        return CacheWrite::Failed;
    }
    g_autofree gchar* joined = g_build_filename(folder.c_str(), name.c_str(), nullptr);
    const std::string path = joined;
    if (holds(path, bytes)) {
        return CacheWrite::Unchanged;
    }
    g_autoptr(GError) error = nullptr;
    if (g_file_set_contents(path.c_str(), bytes.data(), static_cast<gssize>(bytes.size()),
                            &error)) {
        return CacheWrite::Written;
    }
    return CacheWrite::Failed;
}

} // namespace vermilion
