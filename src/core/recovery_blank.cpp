// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Whether a document is the blank one the program starts with, still untouched.

#include "core/recovery_blank.h"

namespace vermilion {

bool is_untouched_blank(const DocumentRecord& record, std::uint64_t chars) {
    return record.path.empty() && !record.modified && !record.read_only && chars == 0;
}

} // namespace vermilion
