// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for text_format: charset names, status bar labels and the majority line ending.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/text_format.h"

namespace {

using vermilion::LineEnding;
using vermilion::LineEndingCounts;
using vermilion::TextEncoding;
using vermilion::TextFormat;

LineEndingCounts counts(std::uint64_t lf, std::uint64_t crlf, std::uint64_t cr) {
    LineEndingCounts c;
    c.lf = lf;
    c.crlf = crlf;
    c.cr = cr;
    return c;
}

TextFormat format_of(TextEncoding encoding, bool bom) {
    TextFormat format;
    format.encoding = encoding;
    format.bom = bom;
    return format;
}

TEST(TextFormat, CharsetNamesForGlib) {
    EXPECT_STREQ(vermilion::charset_name(TextEncoding::Utf8), "UTF-8");
    EXPECT_STREQ(vermilion::charset_name(TextEncoding::Utf16Le), "UTF-16LE");
    EXPECT_STREQ(vermilion::charset_name(TextEncoding::Utf16Be), "UTF-16BE");
    EXPECT_STREQ(vermilion::charset_name(TextEncoding::Windows1252), "WINDOWS-1252");
    EXPECT_STREQ(vermilion::charset_name(TextEncoding::Latin1), "ISO-8859-1");
}

TEST(TextFormat, EncodingLabels) {
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Utf8, false)),
              std::string("UTF-8"));
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Utf8, true)),
              std::string("UTF-8 with BOM"));
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Utf16Le, true)),
              std::string("UTF-16 LE"));
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Utf16Be, true)),
              std::string("UTF-16 BE"));
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Windows1252, false)),
              std::string("Windows-1252"));
    EXPECT_EQ(vermilion::encoding_label(format_of(TextEncoding::Latin1, false)),
              std::string("Latin-1"));
}

TEST(TextFormat, LineEndingLabels) {
    EXPECT_STREQ(vermilion::line_ending_label(LineEnding::Lf), "LF");
    EXPECT_STREQ(vermilion::line_ending_label(LineEnding::CrLf), "CRLF");
    EXPECT_STREQ(vermilion::line_ending_label(LineEnding::Cr), "CR");
}

TEST(TextFormat, MajorityLineEnding) {
    EXPECT_EQ(vermilion::majority_line_ending(counts(0, 0, 0), LineEnding::CrLf), LineEnding::CrLf);
    EXPECT_EQ(vermilion::majority_line_ending(counts(3, 1, 0), LineEnding::Lf), LineEnding::Lf);
    EXPECT_EQ(vermilion::majority_line_ending(counts(1, 3, 0), LineEnding::Lf), LineEnding::CrLf);
    EXPECT_EQ(vermilion::majority_line_ending(counts(1, 1, 5), LineEnding::Lf), LineEnding::Cr);
}

TEST(TextFormat, MajorityTiesGoToLfThenCrLf) {
    EXPECT_EQ(vermilion::majority_line_ending(counts(2, 2, 2), LineEnding::Cr), LineEnding::Lf);
    EXPECT_EQ(vermilion::majority_line_ending(counts(0, 2, 2), LineEnding::Lf), LineEnding::CrLf);
    EXPECT_EQ(vermilion::majority_line_ending(counts(2, 0, 2), LineEnding::CrLf), LineEnding::Lf);
}

TEST(TextFormat, MixedLineEndings) {
    EXPECT_FALSE(vermilion::has_mixed_line_endings(counts(0, 0, 0)));
    EXPECT_FALSE(vermilion::has_mixed_line_endings(counts(5, 0, 0)));
    EXPECT_FALSE(vermilion::has_mixed_line_endings(counts(0, 0, 7)));
    EXPECT_TRUE(vermilion::has_mixed_line_endings(counts(1, 1, 0)));
    EXPECT_TRUE(vermilion::has_mixed_line_endings(counts(0, 4, 1)));
}

} // namespace
