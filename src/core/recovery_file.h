// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The recovery file of an edited document: its header, how it is written and how it is checked.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.h"
#include "core/text_format.h"

namespace vermilion {

// A recovery file is a header, one empty line and then the body: exactly `length` bytes, the
// text of the document as the editor holds it (UTF-8, never converted to the document's own
// encoding). The header is text in UTF-8 lines that end in LF:
//
//   VERMILION-RECOVERY 1
//   uri: file:///home/adam/main.cpp
//   copied: 1794452733554000
//   file-mtime: 1794450001123456
//   encoding: utf-8
//   line-ending: lf
//   length: 18342
//   <empty line>
//
// A line after the first is "key: value". A key is one or more of the letters a to z and "-".
// Keys, in the order encode_header writes them:
//   uri          Percent-encoded URI of the file, starting "file:///". Exactly one of uri and
//                title is present.
//   title        Name of an untitled document, for example "Untitled 3".
//   copied       When the copy was made: Unix time in microseconds, UTC. Required.
//   file-mtime   The file's modified time when the document was loaded or last saved, in the same
//                unit. Optional (not present for an untitled document).
//   encoding     encoding_id of the format: "utf-8", "utf-8-bom", "utf-16le", "utf-16be",
//                "windows-1252" or "latin-1". Required.
//   line-ending  line_ending_id of the ending the next save writes: "lf", "crlf" or "cr".
//                Required.
//   mixed        "yes" when the text has more than one kind of line ending. Written only then.
//   length       Size of the body in bytes. Required.
// Times and length are decimal digits only. A key that is not listed is ignored when reading, so
// a later version can add fields; a repeated key (known or not) is an error.
//
// Limits: the whole header, with its empty line, and one line without its LF. A path of 4096
// bytes that is percent-encoded in full is about 12 KB, so a line has room for it.
inline constexpr std::size_t kRecoveryMaxHeaderBytes = 32 * 1024;
inline constexpr std::size_t kRecoveryMaxLineBytes = 16 * 1024;

struct RecoveryHeader {
    std::optional<std::string> uri;
    std::optional<std::string> title;
    std::int64_t copied_us = 0;
    std::optional<std::int64_t> file_mtime_us;
    TextFormat format;
    bool mixed_line_endings = false;
    std::uint64_t length = 0;
};

struct RecoveryParse {
    RecoveryHeader header;
    std::size_t body_offset = 0; // where the body starts: just after the header's empty line
};

// The header text, ending with its empty line, ready to be followed by the body. Nothing when the
// header cannot be written: both or neither of uri and title; a uri that does not start with
// "file:///"; an empty uri or title; a CR, LF or NUL inside the uri or title; a negative time; a
// line longer than kRecoveryMaxLineBytes. Only a uri or a title can be that long, so a header
// that is written never passes kRecoveryMaxHeaderBytes.
std::optional<std::string> encode_header(const RecoveryHeader& header);

// Reads the header at the start of prefix, which is the beginning of the file (the whole file, or
// at least its first kRecoveryMaxHeaderBytes bytes) and checks it against file_size, the size of
// the whole file: the file must be exactly the header plus `length` bytes. The body itself is
// never read. An error, with a short reason in its message, for anything that is not a version 1
// header exactly as described above: other text or another version number, a header or line that
// is too long or has no empty line, a CR or NUL in the header, a line without ": ", a key with a
// character outside a to z and "-", a repeated key, a missing required key, neither or both of uri
// and title, a uri that does not start with "file:///", an empty title, a time or length that is
// not 1 to 18 (length: 1 to 15) decimal digits, an unknown encoding or line-ending id, a mixed
// that is not "yes", or a file size that is not body_offset + length.
Result<RecoveryParse> parse_header(std::string_view prefix, std::uint64_t file_size);

} // namespace vermilion
