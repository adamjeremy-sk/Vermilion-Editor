// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Looks at a file's state, and optionally hashes its bytes, off the main loop.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "core/file_stamp.h"

namespace vermilion::ui {

// What looking at a file returned.
struct FileInspection {
    // False when the state could not be read for a reason other than the file being missing
    // (permissions, an I/O error, a path that runs through a file). `disk` and `hash` mean
    // nothing then.
    bool readable = false;
    // exists is false for a file that is missing; its time and size are zero then.
    DiskState disk;
    // The XXH3 hash of the file's bytes: only when asked for, only when the whole file was read,
    // and only when the size it had after the read equals the size seen before it (a file that
    // changed while it was read has no hash).
    std::optional<std::uint64_t> hash;
};

using InspectDone = std::function<void(const FileInspection&)>;

// Reads the state of the file at path (absolute, as the file system names it) on a GIO worker
// thread: whether it exists, its modified time (microseconds since the Unix epoch, UTC) and its
// size; with with_hash also the hash of its bytes (a missing or unreadable file has none).
// `done` runs once, from the main context of the thread that called this, always later (never
// from inside this call). The work is not cancelled: a caller that may be gone before the answer
// arrives guards its callback (gtkx::AsyncGuard). Main thread only.
void inspect_file(const std::string& path, bool with_hash, InspectDone done);

} // namespace vermilion::ui
