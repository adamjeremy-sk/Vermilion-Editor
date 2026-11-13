// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery file header: writing it, and reading back what was written.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/recovery_file.h"
#include "core/text_format.h"
#include "recovery_file_test_util.h"

namespace {

using recovery_test::expect_same;
using recovery_test::file_header;
using recovery_test::file_of;
using recovery_test::parse_whole;
using recovery_test::untitled_header;
using recovery_test::valid_file;
using recovery_test::valid_lines;
using vermilion::RecoveryHeader;
using vermilion::RecoveryParse;
using vermilion::Result;
using vermilion::TextFormat;

TEST(RecoveryFile, EncodesAnUntitledHeader) {
    std::string expected = "VERMILION-RECOVERY 1\ntitle: Untitled 3\ncopied: 1794452733554000\n"
                           "encoding: utf-8\nline-ending: lf\nlength: 12\n\n";
    EXPECT_EQ(vermilion::encode_header(untitled_header(12)), std::optional<std::string>(expected));
}

TEST(RecoveryFile, EncodesAFileHeaderWithEveryField) {
    std::string expected =
        "VERMILION-RECOVERY 1\nuri: file:///home/adam/Projects/demo/main.cpp\n"
        "copied: 1794452733554000\nfile-mtime: 1794450001123456\nencoding: utf-16le\n"
        "line-ending: crlf\nmixed: yes\nlength: 18342\n\n";
    EXPECT_EQ(vermilion::encode_header(file_header(18342)), std::optional<std::string>(expected));
}

TEST(RecoveryFile, RoundTripsBothKinds) {
    for (RecoveryHeader header : {untitled_header(7), file_header(7)}) {
        std::optional<std::string> encoded = vermilion::encode_header(header);
        ASSERT_TRUE(encoded.has_value());
        Result<RecoveryParse> parsed = parse_whole(*encoded + "abcdefg");
        ASSERT_TRUE(parsed.is_ok());
        expect_same(parsed.value().header, header);
        EXPECT_EQ(parsed.value().body_offset, encoded->size());
    }
}

TEST(RecoveryFile, RoundTripsEveryEncodingAndLineEnding) {
    const char* const encodings[] = {"utf-8",    "utf-8-bom",    "utf-16le",
                                     "utf-16be", "windows-1252", "latin-1"};
    const char* const endings[] = {"lf", "crlf", "cr"};
    for (const char* encoding : encodings) {
        for (const char* ending : endings) {
            RecoveryHeader header = untitled_header(0);
            header.format = *vermilion::format_with_encoding(TextFormat{}, encoding);
            header.format.line_ending = *vermilion::line_ending_from_id(ending);
            std::optional<std::string> encoded = vermilion::encode_header(header);
            ASSERT_TRUE(encoded.has_value());
            Result<RecoveryParse> parsed = parse_whole(*encoded);
            ASSERT_TRUE(parsed.is_ok());
            expect_same(parsed.value().header, header);
        }
    }
}

TEST(RecoveryFile, TheBodyIsNotLookedAt) {
    const char raw[] = "a\n\nb\r\n\0c\xff\xfe\n\n";
    std::string body(raw, sizeof(raw) - 1);
    RecoveryHeader header = untitled_header(body.size());
    std::string file = *vermilion::encode_header(header) + body;
    Result<RecoveryParse> parsed = parse_whole(file);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(file.substr(parsed.value().body_offset), body);
}

TEST(RecoveryFile, ReadsTheHeaderFromThePrefixOfALargeFile) {
    std::string body(1024 * 1024, 'x');
    std::string head = *vermilion::encode_header(file_header(body.size()));
    std::uint64_t size = head.size() + body.size();
    Result<RecoveryParse> from_prefix =
        vermilion::parse_header((head + body).substr(0, vermilion::kRecoveryMaxHeaderBytes), size);
    ASSERT_TRUE(from_prefix.is_ok());
    EXPECT_EQ(from_prefix.value().body_offset, head.size());
    EXPECT_TRUE(vermilion::parse_header(head, size).is_ok());
    EXPECT_FALSE(vermilion::parse_header(head.substr(0, head.size() - 1), size).is_ok());
}

TEST(RecoveryFile, IgnoresAnUnknownKey) {
    std::vector<std::string> lines = valid_lines();
    lines.push_back("future-thing: anything: goes");
    lines.push_back("other: ");
    Result<RecoveryParse> parsed = parse_whole(file_of(lines, "12345"));
    ASSERT_TRUE(parsed.is_ok());
    Result<RecoveryParse> plain = parse_whole(valid_file());
    ASSERT_TRUE(plain.is_ok());
    expect_same(parsed.value().header, plain.value().header);
}

TEST(RecoveryFile, AnEmptyBodyIsFine) {
    std::vector<std::string> lines = valid_lines();
    lines[4] = "length: 0";
    std::string file = file_of(lines);
    Result<RecoveryParse> parsed = parse_whole(file);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().body_offset, file.size());
    EXPECT_EQ(parsed.value().header.length, std::uint64_t(0));
}

TEST(RecoveryFile, EncodeRefusesAHeaderThatCannotBeRead) {
    RecoveryHeader both = untitled_header(1);
    both.uri = "file:///a.txt";
    EXPECT_FALSE(vermilion::encode_header(both).has_value());
    RecoveryHeader neither = untitled_header(1);
    neither.title.reset();
    EXPECT_FALSE(vermilion::encode_header(neither).has_value());
    for (const std::string& uri :
         {std::string("http://example.com/a"), std::string("/home/a"), std::string("file:///a\nb"),
          std::string("file:///a\rb"), std::string("file:///a\0b", 11)}) {
        RecoveryHeader header = file_header(1);
        header.uri = uri;
        EXPECT_FALSE(vermilion::encode_header(header).has_value()) << uri;
    }
    for (const std::string& title :
         {std::string(), std::string("a\nb"), std::string("a\rb"), std::string("a\0b", 3)}) {
        RecoveryHeader header = untitled_header(1);
        header.title = title;
        EXPECT_FALSE(vermilion::encode_header(header).has_value());
    }
    RecoveryHeader early = untitled_header(1);
    early.copied_us = -1;
    EXPECT_FALSE(vermilion::encode_header(early).has_value());
    RecoveryHeader old = file_header(1);
    old.file_mtime_us = -1;
    EXPECT_FALSE(vermilion::encode_header(old).has_value());
}

TEST(RecoveryFile, EncodeKeepsALongestPathAndRefusesOneMoreByte) {
    std::string longest = "file:///" + std::string(vermilion::kRecoveryMaxLineBytes - 5 - 8, 'x');
    RecoveryHeader header = file_header(3);
    header.uri = longest;
    std::optional<std::string> encoded = vermilion::encode_header(header);
    ASSERT_TRUE(encoded.has_value());
    Result<RecoveryParse> parsed = parse_whole(*encoded + "abc");
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().header.uri, std::optional<std::string>(longest));
    header.uri = longest + "x";
    EXPECT_FALSE(vermilion::encode_header(header).has_value());
    std::string escaped = "file:///";
    for (int i = 0; i < 4096; ++i) {
        escaped += "%C3";
    }
    header.uri = escaped;
    encoded = vermilion::encode_header(header);
    ASSERT_TRUE(encoded.has_value());
    EXPECT_TRUE(parse_whole(*encoded + "abc").is_ok());
}

TEST(RecoveryFile, EncodeKeepsALongestTitleAndRefusesOneMoreByte) {
    std::string longest(vermilion::kRecoveryMaxLineBytes - 7, 'x');
    RecoveryHeader header = untitled_header(3);
    header.title = longest;
    std::optional<std::string> encoded = vermilion::encode_header(header);
    ASSERT_TRUE(encoded.has_value());
    Result<RecoveryParse> parsed = parse_whole(*encoded + "abc");
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().header.title, std::optional<std::string>(longest));
    header.title = longest + "x";
    EXPECT_FALSE(vermilion::encode_header(header).has_value());
}

} // namespace
