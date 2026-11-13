// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Helpers shared by the recovery file tests: sample headers, whole files and a header comparison.

#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/recovery_file.h"
#include "core/text_format.h"

namespace recovery_test {

inline vermilion::RecoveryHeader untitled_header(std::uint64_t length) {
    vermilion::RecoveryHeader header;
    header.title = "Untitled 3";
    header.copied_us = 1794452733554000;
    header.length = length;
    return header;
}

inline vermilion::RecoveryHeader file_header(std::uint64_t length) {
    vermilion::RecoveryHeader header;
    header.uri = "file:///home/adam/Projects/demo/main.cpp";
    header.copied_us = 1794452733554000;
    header.file_mtime_us = 1794450001123456;
    header.format.encoding = vermilion::TextEncoding::Utf16Le;
    header.format.bom = true;
    header.format.line_ending = vermilion::LineEnding::CrLf;
    header.mixed_line_endings = true;
    header.length = length;
    return header;
}

// A whole file: the version line, the given header lines, the empty line, then the body.
inline std::string file_of(const std::vector<std::string>& lines, const std::string& body = "") {
    std::string text = "VERMILION-RECOVERY 1\n";
    for (const std::string& line : lines) {
        text += line + "\n";
    }
    return text + "\n" + body;
}

// The lines of a valid header for a body of five bytes; the index of each line is used below.
inline std::vector<std::string> valid_lines() {
    return {"title: Untitled 1", "copied: 100", "encoding: utf-8", "line-ending: lf", "length: 5"};
}

inline std::string valid_file() {
    return file_of(valid_lines(), "12345");
}

inline vermilion::Result<vermilion::RecoveryParse> parse_whole(const std::string& file) {
    return vermilion::parse_header(file, file.size());
}

// The file for the valid lines with line number index replaced by line (removed if empty).
inline std::string file_with(std::size_t index, const std::string& line) {
    std::vector<std::string> lines = valid_lines();
    if (line.empty()) {
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
        lines[index] = line;
    }
    return file_of(lines, "12345");
}

inline void expect_same(const vermilion::RecoveryHeader& a, const vermilion::RecoveryHeader& b) {
    EXPECT_EQ(a.uri, b.uri);
    EXPECT_EQ(a.title, b.title);
    EXPECT_EQ(a.copied_us, b.copied_us);
    EXPECT_EQ(a.file_mtime_us, b.file_mtime_us);
    EXPECT_STREQ(vermilion::encoding_id(a.format), vermilion::encoding_id(b.format));
    EXPECT_EQ(a.format.line_ending, b.format.line_ending);
    EXPECT_EQ(a.mixed_line_endings, b.mixed_line_endings);
    EXPECT_EQ(a.length, b.length);
}

} // namespace recovery_test
