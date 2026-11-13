// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The file time a recovered document's text was based on, and when its first save must check it.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vermilion {

// A document rebuilt from a recovery copy has no base time for GtkSourceView's own changed-on-disk
// check, so the editor keeps the file time recorded in the copy (the header's file-mtime) and
// checks it itself at the first save to that file.
struct RecoveredBase {
    std::string path;           // the file the copy belongs to (a local path)
    std::int64_t file_mtime_us; // its time when the copy's text was based on it (microseconds, UTC)
};

// True when a save to save_path must first read the file's time: there is a base, the save goes
// to the base's own file (the same path text) and it is not a Save Anyway (overwrite_changes).
// A save to another path (Save As) is checked by nobody: the base says nothing about that file.
bool needs_base_check(const std::optional<RecoveredBase>& base, const std::string& save_path,
                      bool overwrite_changes);

// True when the file's time now (microseconds since the Unix epoch) differs from the base's, in
// either direction: the save must ask Save Anyway. Nothing means the file is gone or cannot be
// read: that is not a change (a save writes the file again, as the start-up question says).
bool file_changed_since_base(const RecoveredBase& base, std::optional<std::int64_t> current_us);

} // namespace vermilion
