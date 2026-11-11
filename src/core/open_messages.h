// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The texts shown while a file is opened: the question before opening, progress and the result.

#pragma once

#include <cstdint>
#include <string>

#include "core/document_list.h"
#include "core/text_scan.h"

namespace vermilion {

struct OpenQuestion {
    std::string message;
    std::string detail;
};

// The title in curly double quotes: \u201ctitle\u201d.
std::string quoted(const std::string& title);

// The question before opening a file that open_warning() flags; both texts empty for None.
// Binary: "\u201cT\u201d looks like a binary file." / "It contains NUL characters. If you open it
// anyway, it opens read-only."
// LongLine: "\u201cT\u201d has a line of N characters." / "Lines this long can make the editor
// slow or stop responding."
OpenQuestion open_question(const std::string& title, OpenWarning warning,
                           std::uint64_t longest_line);

// "Checking \u201cT\u201d\u2026 P%" while the file is read before opening.
std::string checking_message(const std::string& title, int percent);

// "Loading \u201cT\u201d\u2026 P%" while the editor loads it.
std::string loading_message(const std::string& title, int percent);

// The status message after a file has loaded:
// read-only: "Opened \u201cT\u201d read-only: it contains bytes that cannot be shown as text."
// otherwise "Opened \u201cT\u201d", then " as <encoding label>" when not plain UTF-8 (a UTF-8
// file with a byte-order mark says so), then ". It has mixed line endings; saving makes them all
// <LF|CRLF|CR>" when they are mixed, then ".".
std::string loaded_message(const DocumentRecord& record);

} // namespace vermilion
