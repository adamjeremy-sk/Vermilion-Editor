// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The save check against a document's baseline: when it applies and what it finds.

#include "core/baseline.h"

namespace vermilion {

bool needs_baseline_check(const std::optional<Baseline>& baseline, const std::string& save_path,
                          bool overwrite_changes) {
    return baseline.has_value() && !overwrite_changes && baseline->path == save_path;
}

bool changed_since_baseline(const Baseline& baseline, const DiskState& disk) {
    if (!disk.exists) {
        return false;
    }
    if (disk.mtime_us != baseline.stamp.mtime_us) {
        return true;
    }
    if (!baseline.stamp.time_only && disk.size != baseline.stamp.size) {
        return true;
    }
    return false;
}

} // namespace vermilion
