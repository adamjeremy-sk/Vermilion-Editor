// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Decides when a recovered document's first save checks the file's time, and what the time says.

#include "core/recovered_base.h"

namespace vermilion {

bool needs_base_check(const std::optional<RecoveredBase>& base, const std::string& save_path,
                      bool overwrite_changes) {
    return base.has_value() && !overwrite_changes && base->path == save_path;
}

bool file_changed_since_base(const RecoveredBase& base, std::optional<std::int64_t> current_us) {
    return current_us.has_value() && *current_us != base.file_mtime_us;
}

} // namespace vermilion
