// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A document's record of its file (the baseline) and what a look at the file on disk returns.

#pragma once

#include <cstdint>
#include <optional>

namespace vermilion {

// What a document remembers about its file after every load, reload and own save. The time is in
// microseconds since the Unix epoch (UTC), as everywhere else in the program.
struct FileStamp {
    std::int64_t mtime_us = 0;
    std::uint64_t size = 0;
    std::optional<std::uint64_t> hash; // XXH3 of the file's bytes; empty until it is known

    // A document restored by crash recovery knows only its file's time (the time stored with the
    // copy). Its size and hash are not known, so `size` and `hash` mean nothing while this is true.
    bool time_only = false;
};

// What looking at the file on disk returns: no content, only its state.
struct DiskState {
    bool exists = false;
    std::int64_t mtime_us = 0;
    std::uint64_t size = 0;
};

} // namespace vermilion
