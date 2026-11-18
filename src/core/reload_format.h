// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The format a document has after a reload from disk: the file's, or the user's chosen encoding.

#pragma once

#include "core/text_format.h"
#include "core/text_scan.h"

namespace vermilion {

// The format and the mixed-line-endings flag a document takes when it is reloaded from its file.
struct ReloadFormat {
    TextFormat format;
    bool mixed_line_endings = false;
};

// What a reload gives the document, from its current format, its flag and the scan of the new file.
// Without a chosen encoding everything comes from the scan (the file is read as a fresh open reads
// it). With an encoding the user chose (Encoding > Reopen as) the encoding and the byte-order mark
// stay as they are, and the line ending and the mixed flag come from the scan only when the scan
// found that same encoding: a scan that guessed another encoding counted the line endings of a
// text read the wrong way, so the document keeps its own.
ReloadFormat reload_format(const TextFormat& current, bool current_mixed, bool encoding_chosen,
                           const ScanResult& scan);

} // namespace vermilion
