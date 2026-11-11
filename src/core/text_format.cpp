// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Names of the text formats, their menu ids and the majority rule for line endings.

#include "core/text_format.h"

namespace vermilion {

const char* charset_name(TextEncoding encoding) {
    switch (encoding) {
    case TextEncoding::Utf8:
        return "UTF-8";
    case TextEncoding::Utf16Le:
        return "UTF-16LE";
    case TextEncoding::Utf16Be:
        return "UTF-16BE";
    case TextEncoding::Windows1252:
        return "WINDOWS-1252";
    case TextEncoding::Latin1:
        return "ISO-8859-1";
    }
    return "UTF-8";
}

std::string encoding_label(const TextFormat& format) {
    switch (format.encoding) {
    case TextEncoding::Utf8:
        return format.bom ? "UTF-8 with BOM" : "UTF-8";
    case TextEncoding::Utf16Le:
        return "UTF-16 LE";
    case TextEncoding::Utf16Be:
        return "UTF-16 BE";
    case TextEncoding::Windows1252:
        return "Windows-1252";
    case TextEncoding::Latin1:
        return "Latin-1";
    }
    return "UTF-8";
}

const char* line_ending_label(LineEnding ending) {
    switch (ending) {
    case LineEnding::Lf:
        return "LF";
    case LineEnding::CrLf:
        return "CRLF";
    case LineEnding::Cr:
        return "CR";
    }
    return "LF";
}

LineEnding majority_line_ending(const LineEndingCounts& counts, LineEnding fallback) {
    if (counts.lf == 0 && counts.crlf == 0 && counts.cr == 0) {
        return fallback;
    }
    if (counts.lf >= counts.crlf && counts.lf >= counts.cr) {
        return LineEnding::Lf;
    }
    if (counts.crlf >= counts.cr) {
        return LineEnding::CrLf;
    }
    return LineEnding::Cr;
}

bool has_mixed_line_endings(const LineEndingCounts& counts) {
    int kinds = 0;
    if (counts.lf > 0) {
        ++kinds;
    }
    if (counts.crlf > 0) {
        ++kinds;
    }
    if (counts.cr > 0) {
        ++kinds;
    }
    return kinds >= 2;
}

const char* encoding_id(const TextFormat& format) {
    switch (format.encoding) {
    case TextEncoding::Utf8:
        return format.bom ? "utf-8-bom" : "utf-8";
    case TextEncoding::Utf16Le:
        return "utf-16le";
    case TextEncoding::Utf16Be:
        return "utf-16be";
    case TextEncoding::Windows1252:
        return "windows-1252";
    case TextEncoding::Latin1:
        return "latin-1";
    }
    return "utf-8";
}

std::optional<TextFormat> format_with_encoding(const TextFormat& current, const std::string& id) {
    TextFormat format = current;
    if (id == "utf-8") {
        format.encoding = TextEncoding::Utf8;
        format.bom = false;
        return format;
    }
    if (id == "utf-8-bom") {
        format.encoding = TextEncoding::Utf8;
        format.bom = true;
        return format;
    }
    if (id == "utf-16le") {
        format.encoding = TextEncoding::Utf16Le;
        format.bom = true;
        return format;
    }
    if (id == "utf-16be") {
        format.encoding = TextEncoding::Utf16Be;
        format.bom = true;
        return format;
    }
    if (id == "windows-1252") {
        format.encoding = TextEncoding::Windows1252;
        format.bom = false;
        return format;
    }
    if (id == "latin-1") {
        format.encoding = TextEncoding::Latin1;
        format.bom = false;
        return format;
    }
    return std::nullopt;
}

const char* line_ending_id(LineEnding ending) {
    switch (ending) {
    case LineEnding::Lf:
        return "lf";
    case LineEnding::CrLf:
        return "crlf";
    case LineEnding::Cr:
        return "cr";
    }
    return "lf";
}

std::optional<LineEnding> line_ending_from_id(const std::string& id) {
    if (id == "lf") {
        return LineEnding::Lf;
    }
    if (id == "crlf") {
        return LineEnding::CrLf;
    }
    if (id == "cr") {
        return LineEnding::Cr;
    }
    return std::nullopt;
}

} // namespace vermilion
