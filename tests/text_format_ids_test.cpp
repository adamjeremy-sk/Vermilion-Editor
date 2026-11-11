// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the Encoding and Line Ending menu identifiers in text_format.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "core/text_format.h"

namespace {

using vermilion::LineEnding;
using vermilion::TextEncoding;
using vermilion::TextFormat;

TextFormat format_of(TextEncoding encoding, bool bom, LineEnding ending) {
    TextFormat format;
    format.encoding = encoding;
    format.bom = bom;
    format.line_ending = ending;
    return format;
}

TEST(TextFormatIds, EncodingIds) {
    EXPECT_STREQ(vermilion::encoding_id(format_of(TextEncoding::Utf8, false, LineEnding::Lf)),
                 "utf-8");
    EXPECT_STREQ(vermilion::encoding_id(format_of(TextEncoding::Utf8, true, LineEnding::Lf)),
                 "utf-8-bom");
    EXPECT_STREQ(vermilion::encoding_id(format_of(TextEncoding::Utf16Le, true, LineEnding::Lf)),
                 "utf-16le");
    EXPECT_STREQ(vermilion::encoding_id(format_of(TextEncoding::Utf16Be, true, LineEnding::Lf)),
                 "utf-16be");
    EXPECT_STREQ(
        vermilion::encoding_id(format_of(TextEncoding::Windows1252, false, LineEnding::Lf)),
        "windows-1252");
    EXPECT_STREQ(vermilion::encoding_id(format_of(TextEncoding::Latin1, false, LineEnding::Lf)),
                 "latin-1");
}

TEST(TextFormatIds, FormatWithEncodingKeepsLineEnding) {
    TextFormat current = format_of(TextEncoding::Windows1252, false, LineEnding::CrLf);
    std::optional<TextFormat> utf16 = vermilion::format_with_encoding(current, "utf-16be");
    ASSERT_TRUE(utf16.has_value());
    EXPECT_EQ(utf16->encoding, TextEncoding::Utf16Be);
    EXPECT_TRUE(utf16->bom);
    EXPECT_EQ(utf16->line_ending, LineEnding::CrLf);
    std::optional<TextFormat> bom = vermilion::format_with_encoding(current, "utf-8-bom");
    ASSERT_TRUE(bom.has_value());
    EXPECT_EQ(bom->encoding, TextEncoding::Utf8);
    EXPECT_TRUE(bom->bom);
}

TEST(TextFormatIds, FormatWithEncodingDropsTheMark) {
    TextFormat current = format_of(TextEncoding::Utf16Le, true, LineEnding::Cr);
    std::optional<TextFormat> plain = vermilion::format_with_encoding(current, "utf-8");
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->encoding, TextEncoding::Utf8);
    EXPECT_FALSE(plain->bom);
    EXPECT_EQ(plain->line_ending, LineEnding::Cr);
    std::optional<TextFormat> latin = vermilion::format_with_encoding(current, "latin-1");
    ASSERT_TRUE(latin.has_value());
    EXPECT_EQ(latin->encoding, TextEncoding::Latin1);
    EXPECT_FALSE(latin->bom);
    std::optional<TextFormat> cp1252 = vermilion::format_with_encoding(current, "windows-1252");
    ASSERT_TRUE(cp1252.has_value());
    EXPECT_EQ(cp1252->encoding, TextEncoding::Windows1252);
    EXPECT_FALSE(cp1252->bom);
}

TEST(TextFormatIds, EveryEncodingIdRoundTrips) {
    const char* const ids[] = {"utf-8",    "utf-8-bom",    "utf-16le",
                               "utf-16be", "windows-1252", "latin-1"};
    for (const char* id : ids) {
        std::optional<TextFormat> format = vermilion::format_with_encoding(TextFormat{}, id);
        ASSERT_TRUE(format.has_value());
        EXPECT_STREQ(vermilion::encoding_id(*format), id);
    }
}

TEST(TextFormatIds, Utf16AlwaysHasTheMark) {
    std::optional<TextFormat> le = vermilion::format_with_encoding(TextFormat{}, "utf-16le");
    std::optional<TextFormat> be = vermilion::format_with_encoding(TextFormat{}, "utf-16be");
    ASSERT_TRUE(le.has_value());
    ASSERT_TRUE(be.has_value());
    EXPECT_TRUE(le->bom);
    EXPECT_TRUE(be->bom);
}

TEST(TextFormatIds, UnknownEncodingId) {
    EXPECT_FALSE(vermilion::format_with_encoding(TextFormat{}, "UTF-8").has_value());
    EXPECT_FALSE(vermilion::format_with_encoding(TextFormat{}, "").has_value());
}

TEST(TextFormatIds, LineEndingIds) {
    EXPECT_STREQ(vermilion::line_ending_id(LineEnding::Lf), "lf");
    EXPECT_STREQ(vermilion::line_ending_id(LineEnding::CrLf), "crlf");
    EXPECT_STREQ(vermilion::line_ending_id(LineEnding::Cr), "cr");
    EXPECT_EQ(vermilion::line_ending_from_id("lf"), std::optional<LineEnding>(LineEnding::Lf));
    EXPECT_EQ(vermilion::line_ending_from_id("crlf"), std::optional<LineEnding>(LineEnding::CrLf));
    EXPECT_EQ(vermilion::line_ending_from_id("cr"), std::optional<LineEnding>(LineEnding::Cr));
    EXPECT_FALSE(vermilion::line_ending_from_id("CRLF").has_value());
    EXPECT_FALSE(vermilion::line_ending_from_id("").has_value());
}

} // namespace
