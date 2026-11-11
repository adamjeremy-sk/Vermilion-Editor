// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// How a text file is stored on disk: encoding, byte-order mark and line ending, with their names.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vermilion {

// The encodings Vermilion reads and writes. UTF-16 is only recognised by its byte-order mark.
enum class TextEncoding { Utf8, Utf16Le, Utf16Be, Windows1252, Latin1 };

enum class LineEnding { Lf, CrLf, Cr };

struct TextFormat {
    TextEncoding encoding = TextEncoding::Utf8;
    // The file starts with a byte-order mark. It stays in the text as U+FEFF, so it is written
    // back as it was read. Always true for UTF-16, never for Windows-1252 and Latin-1.
    bool bom = false;
    LineEnding line_ending = LineEnding::Lf; // the ending a save writes on every line
};

struct LineEndingCounts {
    std::uint64_t lf = 0;
    std::uint64_t crlf = 0;
    std::uint64_t cr = 0;
};

// Charset name for GLib and GtkSourceView: "UTF-8", "UTF-16LE", "UTF-16BE", "WINDOWS-1252",
// "ISO-8859-1".
const char* charset_name(TextEncoding encoding);

// Name shown in the status bar: "UTF-8", "UTF-8 with BOM", "UTF-16 LE", "UTF-16 BE",
// "Windows-1252", "Latin-1".
std::string encoding_label(const TextFormat& format);

// "LF", "CRLF" or "CR".
const char* line_ending_label(LineEnding ending);

// The ending most lines use. A tie goes to LF, then CRLF, then CR; a text without any line
// ending gives fallback.
LineEnding majority_line_ending(const LineEndingCounts& counts, LineEnding fallback);

// More than one kind of line ending occurs.
bool has_mixed_line_endings(const LineEndingCounts& counts);

// Stable identifiers of the Encoding menu's choices, used as action targets: "utf-8",
// "utf-8-bom", "utf-16le", "utf-16be", "windows-1252", "latin-1".
const char* encoding_id(const TextFormat& format);

// current with the encoding and byte-order mark that id names (UTF-16 always has the mark,
// Windows-1252 and Latin-1 never); the line ending is kept. Nothing for an unknown id.
std::optional<TextFormat> format_with_encoding(const TextFormat& current, const std::string& id);

// Stable identifiers of the Line Ending menu's choices: "lf", "crlf", "cr".
const char* line_ending_id(LineEnding ending);

// The line ending id names, or nothing for an unknown id.
std::optional<LineEnding> line_ending_from_id(const std::string& id);

} // namespace vermilion
