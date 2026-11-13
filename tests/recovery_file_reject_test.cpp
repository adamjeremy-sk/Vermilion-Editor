// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery file header: every kind of damaged input is refused, never trusted.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/recovery_file.h"
#include "core/text_format.h"
#include "recovery_file_test_util.h"

namespace {

using recovery_test::expect_same;
using recovery_test::file_header;
using recovery_test::file_of;
using recovery_test::file_with;
using recovery_test::parse_whole;
using recovery_test::untitled_header;
using recovery_test::valid_file;
using recovery_test::valid_lines;
using vermilion::RecoveryParse;
using vermilion::Result;

TEST(RecoveryFile, RejectsOtherTextAndOtherVersions) {
    EXPECT_TRUE(parse_whole(valid_file()).is_ok());
    EXPECT_FALSE(parse_whole("").is_ok());
    EXPECT_FALSE(parse_whole("hello\n\n").is_ok());
    EXPECT_FALSE(parse_whole("VERMILION-RECOVERY 1").is_ok());
    EXPECT_FALSE(parse_whole("VERMILION-RECOVERY 1 \n\n").is_ok());
    EXPECT_FALSE(parse_whole("vermilion-recovery 1\n\n").is_ok());
    for (char glue : {'X', '\r', ' '}) {
        std::string glued = valid_file();
        glued[std::string("VERMILION-RECOVERY 1").size()] = glue;
        EXPECT_FALSE(parse_whole(glued).is_ok()) << int(glue);
    }
    std::string other = valid_file();
    other[std::string("VERMILION-RECOVERY ").size()] = '2';
    Result<RecoveryParse> parsed = parse_whole(other);
    EXPECT_FALSE(parsed.is_ok());
    EXPECT_FALSE(parsed.error().message.empty());
}

TEST(RecoveryFile, RejectsAMissingRequiredKey) {
    for (std::size_t index = 0; index < valid_lines().size(); ++index) {
        EXPECT_FALSE(parse_whole(file_with(index, "")).is_ok()) << index;
    }
}

TEST(RecoveryFile, RejectsARepeatedKey) {
    for (const std::string& line : valid_lines()) {
        std::vector<std::string> lines = valid_lines();
        lines.push_back(line);
        EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok()) << line;
    }
    std::vector<std::string> lines = valid_lines();
    lines.push_back("future: a");
    lines.push_back("future: b");
    EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok());
}

// A line the reader does not know is still held to the syntax: key, colon, space, value.
TEST(RecoveryFile, RejectsAMalformedExtraLine) {
    const std::vector<std::string> bad = {
        ": x",       "future x",    "future:x",    "future :x",
        "Future: x", "fu_ture: x",  "fu ture: x",  "futur3: x",
        "future",    "future\t: x", "future: a\r", std::string("future: a\0b", 11),
    };
    for (const std::string& line : bad) {
        std::vector<std::string> lines = valid_lines();
        lines.push_back(line);
        EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok()) << line;
    }
}

TEST(RecoveryFile, RejectsBadValues) {
    const std::vector<std::pair<std::size_t, std::string>> bad = {
        {1, "copied: abc"},
        {1, "copied: "},
        {1, "copied: -5"},
        {1, "copied: +5"},
        {1, "copied:  5"},
        {1, "copied: 5 "},
        {1, "copied: 1234567890123456789"},
        {1, "copied=5"},
        {1, "copied:5"},
        {1, "copied : 5"},
        {1, "Copied: 5"},
        {1, "copied_x: 5"},
        {1, ": 5"},
        {1, std::string("copied: 5\r")},
        {1, std::string("copied: 5\0", 10)},
        {2, "encoding: UTF-8"},
        {2, "encoding: utf-16"},
        {2, "encoding: "},
        {3, "line-ending: CRLF"},
        {3, "line-ending: "},
        {4, "length: 1e3"},
        {4, "length: -5"},
        {4, "length: "},
        {4, "length: 0000000000000005 "},
        {0, "title: "},
        {0, "uri: "},
        {0, "uri: http://example.com/a"},
        {0, "uri: /home/adam/a.txt"},
        {0, "uri: file:/a.txt"},
        {0, "uri: file://host/a.txt"},
    };
    for (const std::pair<std::size_t, std::string>& item : bad) {
        EXPECT_FALSE(parse_whole(file_with(item.first, item.second)).is_ok()) << item.second;
    }
}

TEST(RecoveryFile, AcceptsNumbersUpToTheirLimit) {
    // Times: at most 18 digits. Length: at most 15 digits. Leading zeros count as digits.
    EXPECT_TRUE(parse_whole(file_with(1, "copied: " + std::string(15, '0') + "100")).is_ok());
    EXPECT_FALSE(parse_whole(file_with(1, "copied: " + std::string(16, '0') + "100")).is_ok());
    EXPECT_TRUE(parse_whole(file_with(1, "copied: 999999999999999999")).is_ok());
    EXPECT_TRUE(parse_whole(file_with(4, "length: " + std::string(14, '0') + "5")).is_ok());
    EXPECT_FALSE(parse_whole(file_with(4, "length: " + std::string(15, '0') + "5")).is_ok());
    EXPECT_EQ(parse_whole(file_with(1, "copied: 999999999999999999")).value().header.copied_us,
              std::int64_t(999999999999999999));
}

TEST(RecoveryFile, TheMixedKeyIsYesOrAbsent) {
    std::vector<std::string> lines = valid_lines();
    lines.push_back("mixed: yes");
    Result<RecoveryParse> yes = parse_whole(file_of(lines, "12345"));
    ASSERT_TRUE(yes.is_ok());
    EXPECT_TRUE(yes.value().header.mixed_line_endings);
    EXPECT_FALSE(parse_whole(valid_file()).value().header.mixed_line_endings);
    for (const char* bad : {"mixed: no", "mixed: true", "mixed: Yes", "mixed: ", "mixed: yes "}) {
        lines.back() = bad;
        EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok()) << bad;
    }
}

TEST(RecoveryFile, NeedsExactlyOneOfUriAndTitle) {
    std::vector<std::string> lines = valid_lines();
    lines.push_back("uri: file:///a.txt");
    EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok());
    lines[0] = "uri: file:///a.txt";
    lines.pop_back();
    Result<RecoveryParse> parsed = parse_whole(file_of(lines, "12345"));
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().header.uri, std::optional<std::string>("file:///a.txt"));
    EXPECT_FALSE(parsed.value().header.title.has_value());
}

TEST(RecoveryFile, TheFileMustBeExactlyHeaderPlusLength) {
    std::string file = valid_file();
    EXPECT_TRUE(vermilion::parse_header(file, file.size()).is_ok());
    EXPECT_FALSE(vermilion::parse_header(file, file.size() + 1).is_ok());
    EXPECT_FALSE(vermilion::parse_header(file, file.size() - 1).is_ok());
    EXPECT_FALSE(vermilion::parse_header(file, 0).is_ok());
    EXPECT_FALSE(parse_whole(file + "x").is_ok());
    EXPECT_FALSE(parse_whole(file.substr(0, file.size() - 1)).is_ok());
}

TEST(RecoveryFile, RejectsAHeaderWithoutItsEmptyLine) {
    std::string file = valid_file();
    std::string cut = file.substr(0, file.find("\n\n") + 1) + "12345";
    EXPECT_FALSE(parse_whole(cut).is_ok());
    EXPECT_FALSE(parse_whole("VERMILION-RECOVERY 1\n").is_ok());
}

TEST(RecoveryFile, RejectsALineThatIsTooLong) {
    std::vector<std::string> lines = valid_lines();
    std::string head = "future: ";
    lines.push_back(head + std::string(vermilion::kRecoveryMaxLineBytes - head.size(), 'x'));
    EXPECT_TRUE(parse_whole(file_of(lines, "12345")).is_ok());
    lines.back() += "x";
    EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok());
}

TEST(RecoveryFile, TheHeaderMayFillExactlyItsLimit) {
    std::size_t base = valid_file().size() - 5;
    const std::size_t kBig = 10000;
    std::size_t rest = vermilion::kRecoveryMaxHeaderBytes - base - 2 * (kBig + 1) - 1;
    for (std::size_t extra : {std::size_t(0), std::size_t(1)}) {
        std::vector<std::string> lines = valid_lines();
        lines.push_back("pad-a: " + std::string(kBig - 7, 'x'));
        lines.push_back("pad-b: " + std::string(kBig - 7, 'x'));
        lines.push_back("pad-c: " + std::string(rest + extra - 7, 'x'));
        EXPECT_EQ(parse_whole(file_of(lines, "12345")).is_ok(), extra == 0) << extra;
    }
}

TEST(RecoveryFile, RejectsAHeaderOfManyLines) {
    std::vector<std::string> lines = valid_lines();
    for (int i = 0; i < 4000; ++i) {
        std::string key = "k";
        for (int n = i; n > 0; n /= 26) {
            key += static_cast<char>('a' + n % 26);
        }
        lines.push_back(key + ": value");
    }
    EXPECT_FALSE(parse_whole(file_of(lines, "12345")).is_ok());
}

// A small deterministic random source, so a failure can be repeated.
class Random {
public:
    std::uint32_t next(std::uint32_t below) {
        state_ = state_ * 1664525u + 1013904223u;
        return (state_ >> 8) % below;
    }

private:
    std::uint32_t state_ = 12345;
};

void check_parse_of(const std::string& text, std::uint64_t file_size) {
    Result<RecoveryParse> parsed = vermilion::parse_header(text, file_size);
    if (!parsed.is_ok()) {
        return;
    }
    const RecoveryParse& parse = parsed.value();
    EXPECT_LE(parse.body_offset, text.size());
    EXPECT_EQ(file_size, parse.body_offset + parse.header.length);
    std::optional<std::string> again = vermilion::encode_header(parse.header);
    ASSERT_TRUE(again.has_value());
    std::string rebuilt = *again + std::string(parse.header.length, 'x');
    Result<RecoveryParse> second = parse_whole(rebuilt);
    ASSERT_TRUE(second.is_ok());
    expect_same(second.value().header, parse.header);
}

TEST(RecoveryFile, TheKeysMayComeInAnyOrder) {
    std::vector<std::string> lines = valid_lines();
    lines[2] = "encoding: utf-16be";
    lines[3] = "line-ending: cr";
    Result<RecoveryParse> forward = parse_whole(file_of(lines, "12345"));
    ASSERT_TRUE(forward.is_ok());
    EXPECT_EQ(forward.value().header.format.line_ending, vermilion::LineEnding::Cr);
    const std::vector<std::string> reversed(lines.rbegin(), lines.rend());
    Result<RecoveryParse> backward = parse_whole(file_of(reversed, "12345"));
    ASSERT_TRUE(backward.is_ok());
    expect_same(forward.value().header, backward.value().header);
    EXPECT_STREQ(vermilion::encoding_id(backward.value().header.format), "utf-16be");
    EXPECT_EQ(backward.value().header.format.line_ending, vermilion::LineEnding::Cr);
    EXPECT_EQ(forward.value().body_offset, backward.value().body_offset);
}

TEST(RecoveryFile, DamagedInputNeverBreaksTheReader) {
    Random random;
    std::string alphabet = "VERMILON-CY 1\n:\rabz09+-_/%file:u";
    alphabet.push_back('\0');
    std::vector<std::string> seeds = {valid_file(),
                                      *vermilion::encode_header(file_header(4)) + "abcd",
                                      *vermilion::encode_header(untitled_header(0))};
    for (int round = 0; round < 6000; ++round) {
        std::string text = seeds[random.next(3)];
        for (std::uint32_t edits = random.next(4); edits > 0; --edits) {
            std::size_t at = random.next(static_cast<std::uint32_t>(text.size() + 1));
            char c = alphabet[random.next(static_cast<std::uint32_t>(alphabet.size()))];
            switch (random.next(4)) {
            case 0:
                text.insert(at, 1, c);
                break;
            case 1:
                if (at < text.size()) {
                    text.erase(at, 1);
                }
                break;
            case 2:
                if (at < text.size()) {
                    text[at] = c;
                }
                break;
            default:
                text.resize(at);
                break;
            }
        }
        check_parse_of(text, text.size());
        check_parse_of(text, random.next(64));
    }
    for (int round = 0; round < 3000; ++round) {
        std::string text;
        for (std::uint32_t n = random.next(120); n > 0; --n) {
            text += alphabet[random.next(static_cast<std::uint32_t>(alphabet.size()))];
        }
        check_parse_of(text, text.size());
    }
}

// Without a length the body would count as zero bytes, so a file with no body must still fail.
TEST(RecoveryFile, RejectsAMissingLengthEvenWhenThereIsNoBody) {
    std::vector<std::string> lines = valid_lines();
    lines.pop_back();
    EXPECT_FALSE(parse_whole(file_of(lines)).is_ok());
}

} // namespace
