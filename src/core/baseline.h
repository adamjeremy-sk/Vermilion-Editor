// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A document's baseline (its file's path and stamp) and the check a save makes against the disk.

#pragma once

#include <optional>
#include <string>

#include "core/file_stamp.h"

namespace vermilion {

// What a file document remembers about its file: where it is and its stamp (time, size and hash)
// as they were when the text and the file were last equal (after a load or an own save). A document
// rebuilt from a recovery copy has a time-only stamp (see FileStamp). GtkSourceView's own
// changed-on-disk check is not used: the save always passes its ignore flag and the page checks
// the baseline itself.
struct Baseline {
    std::string path; // the file's local path
    FileStamp stamp;
};

// True when a save to save_path must first look at the file: there is a baseline, the save goes to
// the baseline's own file (the same path text) and it is not a Save Anyway (overwrite_changes). A
// save to another path (Save As) is checked by nobody: the baseline says nothing about that file.
bool needs_baseline_check(const std::optional<Baseline>& baseline, const std::string& save_path,
                          bool overwrite_changes);

// True when the file on disk differs from the baseline: another time, or, for a stamp that knows
// its size, another size. A file that is gone is not a change (a save writes it again).
bool changed_since_baseline(const Baseline& baseline, const DiskState& disk);

} // namespace vermilion
