// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Writes a file into a cache folder only when its content differs, so a start-up that finds the
// file in place does no write.

#pragma once

#include <string>

namespace vermilion {

enum class CacheWrite {
    Unchanged, // the file already held exactly these bytes; nothing was written
    Written,   // the file was missing or different and now holds the bytes
    Failed     // the name is not a plain file name, or the folder or the file could not be made
};

// Makes sure `folder`/`name` holds exactly `bytes`.
// - `name` must be a plain file name: an empty name, "." or "..", and any name holding '/' give
//   Failed without touching the disk.
// - `folder` is made with its parents (mode 0700) when it does not exist.
// - An existing entry counts as holding the bytes only when it is a regular file (not a link, a
//   pipe or a folder; judged with lstat, never by opening it) of exactly bytes.size() bytes with
//   the same content. Anything else is replaced.
// - A write goes to a temporary file in the same folder that is renamed over `name`, so a failure
//   never leaves a half-written file under `name` and no temporary file stays behind.
// Never throws; blocks only on the disk.
CacheWrite write_if_changed(const std::string& folder, const std::string& name,
                            const std::string& bytes);

} // namespace vermilion
