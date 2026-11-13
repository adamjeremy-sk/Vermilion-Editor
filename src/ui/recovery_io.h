// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// File work for the start-up question that must not block the main loop: a file's time, a copy.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "core/recovery_store.h"

namespace vermilion::ui {

// Reads the modified time of the file at path (absolute) in microseconds since the Unix epoch,
// the same way the editor records it (GIO's time::modified, with microseconds), or nothing when
// it cannot be read (the file is gone, or not readable). The read is asynchronous: done runs
// exactly once, from the main loop and never inside this call. It is not guarded: the caller
// makes it harmless when its owner is gone.
void query_file_time(const std::string& path,
                     std::function<void(std::optional<std::int64_t>)> done);

// What reading a copy gave.
struct CopyRead {
    // The copy, when it was read whole and its text passed recovery_text_problem().
    std::optional<RecoveryCopy> copy;
    // When copy is empty: why the copy cannot be used, as a short text for the log and for the
    // damaged-copy question (the store's message, or the text of recovery_text_problem()).
    std::string problem;
};

// Reads the copy with this id from store on a worker thread (RecoveryStore::read, then the check
// of its text), so a copy of many megabytes never holds the main loop. done runs exactly once,
// from the main loop and never inside this call, with the result moved into it. It is not
// guarded: the caller makes it harmless when its owner is gone.
void read_recovery_copy(const RecoveryStore& store, const std::string& id,
                        std::function<void(CopyRead)> done);

} // namespace vermilion::ui
