// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the start-up question says about a recovery copy: its file, its times and the texts.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "core/recovery_file.h"
#include "core/recovery_offer.h"

namespace vermilion {

// The local path of the file a copy belongs to, from the header's uri; nothing for an untitled
// copy (no uri) and for a uri that is not a local file (g_filename_from_uri refuses it).
std::optional<std::string> recovery_path(const RecoveryHeader& header);

// The name the question and the status bar use for a copy's document: the last part of its path
// (as g_filename_display_name shows it), else its title, else its uri, else "Untitled".
std::string recovery_name(const RecoveryHeader& header);

// How the copy's file stands now against the file time the copy's text was based on.
enum class FileState {
    Untitled,  // the copy has no uri: it was never saved to a file
    Unchanged, // the file's time now is the recorded time
    Changed,   // the file's time now differs from the recorded time
    Missing,   // no time could be read now: the file is gone or cannot be read
    Unknown,   // the copy records no file time, so a change cannot be told
};

// current_mtime_us is the file's modified time now in microseconds since the Unix epoch, or
// nothing when it could not be read. Checked in this order: no uri is Untitled, no current time
// is Missing, no recorded time (header.file_mtime_us) is Unknown, equal times are Unchanged, any
// other pair is Changed.
FileState file_state(const RecoveryHeader& header, std::optional<std::int64_t> current_mtime_us);

// A time in microseconds since the Unix epoch as "YYYY-MM-DD HH:MM:SS" (whole seconds, cut off,
// not rounded), in the local time zone or, when local_time is false, in UTC. A negative or out
// of range time gives "an unknown time".
std::string format_recovery_time(std::int64_t microseconds, bool local_time);

struct RecoveryQuestion {
    std::string message;
    std::string detail;
};

// The two texts of the question about one copy, number of total (both counted from the offer; the
// "(N of M)" is added only when total is above 1). current_mtime_us is as in file_state(); it is
// ignored for a damaged item. Every time is written by format_recovery_time(, local_time). Names
// are in curly quotes (quoted() of open_messages.h). The lines of the detail are joined by "\n".
//
// A valid copy: message `Recover unsaved changes to <quoted name>?`, then ` (N of M)`.
//   detail line 1: `The copy was made <time>.`
//   Untitled: line 2 `The document was never saved to a file.`
//   otherwise line 2 `File: <path>` (the display name of recovery_path(), or the uri itself when
//   there is no local path) and line 3 by state:
//     Unchanged: `The file has not changed since the copy's text was based on it.`
//     Changed:   `The file was last changed <current time>, after the copy's text was based on it.
//                 Saving will ask before it overwrites the file.`
//     Missing:   `The file no longer exists or cannot be read. Saving will write it again.`
//     Unknown:   `The copy does not record which version of the file its text was based on. The
//                 file was last changed <current time>.`
// A damaged copy: message `A recovery copy is damaged.`, then ` (N of M)`; detail
//   `The copy <id>.vrec cannot be used: <problem>` and, on a second line,
//   `Decide Later keeps it; Discard deletes it.`
// (The texts in backticks are the exact strings; a wrapped line above is one line in the code.)
RecoveryQuestion recovery_question(const RecoveryItem& item, std::size_t number, std::size_t total,
                                   std::optional<std::int64_t> current_mtime_us,
                                   bool local_time = true);

} // namespace vermilion
