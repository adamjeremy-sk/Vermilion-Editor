// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the recovery file header: writing it, and reading and checking it strictly.

#include "core/recovery_file.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace vermilion {

namespace {

constexpr std::string_view kFirstLine = "VERMILION-RECOVERY 1";
constexpr std::string_view kFirstLineStart = "VERMILION-RECOVERY ";
constexpr std::string_view kUriStart = "file:///";

bool starts_with(std::string_view text, std::string_view start) {
    return text.size() >= start.size() && text.compare(0, start.size(), start) == 0;
}

bool has_line_break_or_nul(std::string_view value) {
    for (char c : value) {
        if (c == '\r' || c == '\n' || c == '\0') {
            return true;
        }
    }
    return false;
}

bool is_key_char(char c) {
    return (c >= 'a' && c <= 'z') || c == '-';
}

std::optional<std::uint64_t> parse_number(std::string_view value, std::size_t max_digits) {
    if (value.empty() || value.size() > max_digits) {
        return std::nullopt;
    }
    std::uint64_t number = 0;
    for (char c : value) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        number = number * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return number;
}

// The structural checks of one header line, in order; on success, value is the text after ": ".
std::optional<std::string> check_line(std::string_view line, std::string_view& value,
                                      std::set<std::string_view>& seen) {
    if (line.size() > kRecoveryMaxLineBytes) {
        return "a line is too long";
    }
    if (has_line_break_or_nul(line)) {
        return "a line holds a line break or NUL";
    }
    std::size_t colon = line.find(": ");
    if (colon == std::string_view::npos || colon == 0) {
        return "a line has no ': '";
    }
    std::string_view key = line.substr(0, colon);
    for (char c : key) {
        if (!is_key_char(c)) {
            return "a key holds a bad character";
        }
    }
    if (!seen.insert(key).second) {
        return "a key is repeated";
    }
    value = line.substr(colon + 2);
    return std::nullopt;
}

// The value of a numeric key (copied, file-mtime, length), or nothing if the key is not numeric
// or the value is not a valid number of the key's width.
std::optional<std::uint64_t> number_value(std::string_view key, std::string_view value) {
    if (key == "copied" || key == "file-mtime") {
        return parse_number(value, 18);
    }
    if (key == "length") {
        return parse_number(value, 15);
    }
    return std::nullopt;
}

// Applies the uri, title, mixed, copied, file-mtime and length keys; nothing when the key is one
// the other helper owns or an unknown key (which is ignored).
std::optional<std::string> apply_plain(std::string_view key, std::string_view value,
                                       RecoveryHeader& header, bool& copied_seen,
                                       bool& length_seen) {
    if (key == "uri") {
        if (!starts_with(value, kUriStart)) {
            return "uri does not start with file:///";
        }
        header.uri = std::string(value);
        return std::nullopt;
    }
    if (key == "title") {
        if (value.empty()) {
            return "title is empty";
        }
        header.title = std::string(value);
        return std::nullopt;
    }
    if (key == "mixed") {
        if (value != "yes") {
            return "mixed is not yes";
        }
        header.mixed_line_endings = true;
        return std::nullopt;
    }
    std::optional<std::uint64_t> number = number_value(key, value);
    if (key == "copied") {
        if (!number) {
            return "copied is not a number";
        }
        header.copied_us = static_cast<std::int64_t>(*number);
        copied_seen = true;
        return std::nullopt;
    }
    if (key == "file-mtime") {
        if (!number) {
            return "file-mtime is not a number";
        }
        header.file_mtime_us = static_cast<std::int64_t>(*number);
        return std::nullopt;
    }
    if (key == "length") {
        if (!number) {
            return "length is not a number";
        }
        header.length = *number;
        length_seen = true;
        return std::nullopt;
    }
    return std::nullopt;
}

// Applies the encoding and line-ending keys; nothing when the key is one the other helper owns or
// an unknown key (which is ignored).
std::optional<std::string> apply_format(std::string_view key, std::string_view value,
                                        RecoveryHeader& header, bool& encoding_seen,
                                        bool& ending_seen) {
    if (key == "encoding") {
        std::optional<TextFormat> format = format_with_encoding(header.format, std::string(value));
        if (!format) {
            return "encoding is unknown";
        }
        header.format = *format;
        encoding_seen = true;
        return std::nullopt;
    }
    if (key == "line-ending") {
        std::optional<LineEnding> ending = line_ending_from_id(std::string(value));
        if (!ending) {
            return "line-ending is unknown";
        }
        header.format.line_ending = *ending;
        ending_seen = true;
        return std::nullopt;
    }
    return std::nullopt;
}

// The checks that hold only once the header has ended: the required keys are present, exactly one
// of uri and title is present, and the file size is the header plus the body.
std::optional<std::string> finish_header(const RecoveryParse& parsed, bool copied_seen,
                                         bool encoding_seen, bool ending_seen, bool length_seen,
                                         std::size_t position, std::uint64_t file_size) {
    if (!copied_seen) {
        return "copied is missing";
    }
    if (!encoding_seen) {
        return "encoding is missing";
    }
    if (!ending_seen) {
        return "line-ending is missing";
    }
    if (!length_seen) {
        return "length is missing";
    }
    if (!parsed.header.uri.has_value() && !parsed.header.title.has_value()) {
        return "neither uri nor title is present";
    }
    if (parsed.header.uri.has_value() && parsed.header.title.has_value()) {
        return "both uri and title are present";
    }
    if (file_size != static_cast<std::uint64_t>(position) + parsed.header.length) {
        return "the file size is not header plus length";
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> encode_header(const RecoveryHeader& header) {
    if (header.uri.has_value() == header.title.has_value()) {
        return std::nullopt;
    }
    std::string_view name;
    std::size_t prefix_size = 0;
    if (header.uri.has_value()) {
        name = *header.uri;
        prefix_size = 5;
        if (!starts_with(name, kUriStart)) {
            return std::nullopt;
        }
    } else {
        name = *header.title;
        prefix_size = 7;
    }
    if (name.empty() || has_line_break_or_nul(name)) {
        return std::nullopt;
    }
    if (prefix_size + name.size() > kRecoveryMaxLineBytes) {
        return std::nullopt;
    }
    if (header.copied_us < 0 || (header.file_mtime_us.has_value() && *header.file_mtime_us < 0)) {
        return std::nullopt;
    }
    std::string text;
    text += std::string(kFirstLine) + "\n";
    if (header.uri.has_value()) {
        text += "uri: " + *header.uri + "\n";
    } else {
        text += "title: " + *header.title + "\n";
    }
    text += "copied: " + std::to_string(header.copied_us) + "\n";
    if (header.file_mtime_us.has_value()) {
        text += "file-mtime: " + std::to_string(*header.file_mtime_us) + "\n";
    }
    text += std::string("encoding: ") + encoding_id(header.format) + "\n";
    text += std::string("line-ending: ") + line_ending_id(header.format.line_ending) + "\n";
    if (header.mixed_line_endings) {
        text += "mixed: yes\n";
    }
    text += "length: " + std::to_string(header.length) + "\n";
    text += "\n";
    return text;
}

Result<RecoveryParse> parse_header(std::string_view prefix, std::uint64_t file_size) {
    auto fail = [](const std::string& reason) {
        return Result<RecoveryParse>::err(Error{"damaged recovery file: " + reason});
    };
    std::size_t first_len = kFirstLine.size() + 1;
    if (!starts_with(prefix, kFirstLine) || prefix.size() <= kFirstLine.size() ||
        prefix[kFirstLine.size()] != '\n') {
        return fail(starts_with(prefix, kFirstLineStart) ? "another version"
                                                         : "not a recovery file");
    }
    std::string_view window = prefix.substr(0, std::min(prefix.size(), kRecoveryMaxHeaderBytes));
    RecoveryParse parsed;
    bool copied_seen = false;
    bool encoding_seen = false;
    bool ending_seen = false;
    bool length_seen = false;
    std::set<std::string_view> seen;
    std::size_t position = first_len;
    while (true) {
        std::size_t lf = window.find('\n', position);
        if (lf == std::string_view::npos) {
            return fail("the header does not end");
        }
        std::string_view line = window.substr(position, lf - position);
        position = lf + 1;
        if (line.empty()) {
            break;
        }
        std::string_view value;
        std::optional<std::string> problem = check_line(line, value, seen);
        if (problem) {
            return fail(*problem);
        }
        std::string_view key = line.substr(0, line.find(": "));
        problem = apply_plain(key, value, parsed.header, copied_seen, length_seen);
        if (!problem) {
            problem = apply_format(key, value, parsed.header, encoding_seen, ending_seen);
        }
        if (problem) {
            return fail(*problem);
        }
    }
    std::optional<std::string> problem = finish_header(
        parsed, copied_seen, encoding_seen, ending_seen, length_seen, position, file_size);
    if (problem) {
        return fail(*problem);
    }
    parsed.body_offset = position;
    return Result<RecoveryParse>::ok(std::move(parsed));
}

} // namespace vermilion
