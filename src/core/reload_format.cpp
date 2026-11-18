// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The format a document has after a reload from disk: the file's, or the user's chosen encoding.

#include "core/reload_format.h"

namespace vermilion {

ReloadFormat reload_format(const TextFormat& current, bool current_mixed, bool encoding_chosen,
                           const ScanResult& scan) {
    if (!encoding_chosen) {
        return ReloadFormat{scan.format, scan.mixed_line_endings};
    }
    ReloadFormat result{current, current_mixed};
    if (scan.format.encoding == current.encoding) {
        result.format.line_ending = scan.format.line_ending;
        result.mixed_line_endings = scan.mixed_line_endings;
    }
    return result;
}

} // namespace vermilion
