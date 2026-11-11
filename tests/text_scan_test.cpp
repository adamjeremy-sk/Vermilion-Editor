// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for TextScanner: encodings, byte-order marks, line endings, long lines and block splits.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/text_scan.h"

namespace {

using vermilion::LineEnding;
using vermilion::OpenWarning;
using vermilion::ScanResult;
using vermilion::TextEncoding;
using vermilion::TextScanner;

ScanResult scan(const std::string& bytes) {
    TextScanner scanner;
    scanner.feed(bytes.data(), bytes.size());
    return scanner.finish();
}

ScanResult scan_blocks(const std::vector<std::string>& blocks) {
    TextScanner scanner;
    for (const std::string& block : blocks) {
        scanner.feed(block.data(), block.size());
    }
    return scanner.finish();
}

bool same(const ScanResult& a, const ScanResult& b) {
    return a.format.encoding == b.format.encoding && a.format.bom == b.format.bom &&
           a.format.line_ending == b.format.line_ending && a.endings.lf == b.endings.lf &&
           a.endings.crlf == b.endings.crlf && a.endings.cr == b.endings.cr &&
           a.mixed_line_endings == b.mixed_line_endings && a.longest_line == b.longest_line &&
           a.binary == b.binary && a.size == b.size;
}

TEST(TextScan, EmptyFileIsPlainUtf8) {
    ScanResult result = scan("");
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_FALSE(result.format.bom);
    EXPECT_EQ(result.format.line_ending, LineEnding::Lf);
    EXPECT_EQ(result.longest_line, 0u);
    EXPECT_FALSE(result.binary);
    EXPECT_FALSE(result.mixed_line_endings);
    EXPECT_EQ(result.size, 0u);
    EXPECT_EQ(vermilion::open_warning(result, vermilion::kDefaultLongLineLimit), OpenWarning::None);
}

TEST(TextScan, LfLinesAndLongestLine) {
    ScanResult result = scan("a\nbb\nccc");
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_EQ(result.endings.lf, 2u);
    EXPECT_EQ(result.endings.crlf, 0u);
    EXPECT_EQ(result.endings.cr, 0u);
    EXPECT_EQ(result.longest_line, 3u);
    EXPECT_EQ(result.size, 8u);
}

TEST(TextScan, CrLfCountsOnceAndNotInLineLength) {
    ScanResult result = scan("ab\r\ncd\r\n");
    EXPECT_EQ(result.endings.crlf, 2u);
    EXPECT_EQ(result.endings.lf, 0u);
    EXPECT_EQ(result.endings.cr, 0u);
    EXPECT_EQ(result.format.line_ending, LineEnding::CrLf);
    EXPECT_FALSE(result.mixed_line_endings);
    EXPECT_EQ(result.longest_line, 2u);
}

TEST(TextScan, LoneCrIsALineEnding) {
    ScanResult result = scan("a\rbcd\r\r");
    EXPECT_EQ(result.endings.cr, 3u);
    EXPECT_EQ(result.endings.lf, 0u);
    EXPECT_EQ(result.format.line_ending, LineEnding::Cr);
    EXPECT_EQ(result.longest_line, 3u);
}

TEST(TextScan, CrAtEndOfFileIsCr) {
    ScanResult result = scan("abc\r");
    EXPECT_EQ(result.endings.cr, 1u);
    EXPECT_EQ(result.endings.crlf, 0u);
}

TEST(TextScan, MixedEndingsUseTheMajority) {
    ScanResult result = scan("a\r\nb\r\nc\nd\r\n");
    EXPECT_EQ(result.endings.crlf, 3u);
    EXPECT_EQ(result.endings.lf, 1u);
    EXPECT_TRUE(result.mixed_line_endings);
    EXPECT_EQ(result.format.line_ending, LineEnding::CrLf);
}

TEST(TextScan, MixedTieGoesToLf) {
    ScanResult result = scan("a\nb\r\n");
    EXPECT_TRUE(result.mixed_line_endings);
    EXPECT_EQ(result.format.line_ending, LineEnding::Lf);
}

TEST(TextScan, CrLfSplitAcrossBlocks) {
    ScanResult result = scan_blocks({"ab\r", "\ncd"});
    EXPECT_EQ(result.endings.crlf, 1u);
    EXPECT_EQ(result.endings.cr, 0u);
    EXPECT_EQ(result.endings.lf, 0u);
    EXPECT_EQ(result.longest_line, 2u);
}

TEST(TextScan, Utf8CountsCodePoints) {
    // "h\u00e9llo \u20ac" is 7 characters in 10 bytes.
    ScanResult result = scan("h\xc3\xa9llo \xe2\x82\xac\nx");
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_EQ(result.longest_line, 7u);
}

TEST(TextScan, MultiByteCharacterSplitAcrossBlocks) {
    ScanResult result = scan_blocks({"ab\xe2", "\x82",
                                     "\xac"
                                     "cd"});
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_EQ(result.longest_line, 5u);
}

TEST(TextScan, Utf8ByteOrderMark) {
    ScanResult result = scan("\xef\xbb\xbf"
                             "ab\ncde");
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_TRUE(result.format.bom);
    EXPECT_EQ(result.longest_line, 3u);
    EXPECT_EQ(result.size, 9u);
}

TEST(TextScan, Utf8ByteOrderMarkSplitAcrossBlocks) {
    ScanResult result = scan_blocks({"\xef", "", "\xbb", "\xbf", "ab"});
    EXPECT_TRUE(result.format.bom);
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
    EXPECT_EQ(result.longest_line, 2u);
}

TEST(TextScan, StartOfAByteOrderMarkIsContent) {
    ScanResult result = scan("\xef\xbb");
    EXPECT_FALSE(result.format.bom);
    EXPECT_EQ(result.format.encoding, TextEncoding::Windows1252);
    EXPECT_EQ(result.longest_line, 2u);
}

TEST(TextScan, Utf16LittleEndian) {
    ScanResult result = scan(std::string("\xff\xfe"
                                         "a\0\n\0b\0c\0",
                                         10));
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf16Le);
    EXPECT_TRUE(result.format.bom);
    EXPECT_EQ(result.endings.lf, 1u);
    EXPECT_EQ(result.longest_line, 2u);
    EXPECT_FALSE(result.binary);
    EXPECT_EQ(result.size, 10u);
}

TEST(TextScan, Utf16BigEndianCrLf) {
    ScanResult result = scan(std::string("\xfe\xff\0a\0\r\0\n\0b", 10));
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf16Be);
    EXPECT_TRUE(result.format.bom);
    EXPECT_EQ(result.endings.crlf, 1u);
    EXPECT_EQ(result.format.line_ending, LineEnding::CrLf);
    EXPECT_EQ(result.longest_line, 1u);
    EXPECT_FALSE(result.binary);
}

TEST(TextScan, Utf16UnitSplitAcrossBlocks) {
    ScanResult result = scan_blocks({"\xff",
                                     std::string("\xfe"
                                                 "a",
                                                 2),
                                     std::string("\0\n", 2), std::string("\0", 1)});
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf16Le);
    EXPECT_EQ(result.endings.lf, 1u);
    EXPECT_FALSE(result.binary);
}

TEST(TextScan, Utf16OddLengthIsBinary) {
    ScanResult result = scan(std::string("\xff\xfe"
                                         "a\0b",
                                         5));
    EXPECT_EQ(result.format.encoding, TextEncoding::Utf16Le);
    EXPECT_TRUE(result.binary);
}

TEST(TextScan, Utf16NulUnitIsBinary) {
    ScanResult result = scan(std::string("\xff\xfe"
                                         "a\0\0\0",
                                         6));
    EXPECT_TRUE(result.binary);
}

TEST(TextScan, NulByteIsBinary) {
    ScanResult result = scan(std::string("ab\0cd", 5));
    EXPECT_TRUE(result.binary);
    EXPECT_EQ(vermilion::open_warning(result, vermilion::kDefaultLongLineLimit),
              OpenWarning::Binary);
}

TEST(TextScan, InvalidUtf8IsWindows1252) {
    ScanResult result = scan("caf\xe9\n");
    EXPECT_EQ(result.format.encoding, TextEncoding::Windows1252);
    EXPECT_FALSE(result.format.bom);
    EXPECT_EQ(result.longest_line, 4u);
}

TEST(TextScan, UndefinedWindows1252ByteGivesLatin1) {
    const char* const samples[] = {"\x81", "\x8d", "\x8f", "\x90", "\x9d"};
    for (const char* sample : samples) {
        ScanResult result = scan(std::string("ab") + sample + "cd");
        EXPECT_EQ(result.format.encoding, TextEncoding::Latin1);
        EXPECT_EQ(result.longest_line, 5u);
    }
}

TEST(TextScan, DefinedWindows1252BytesStayWindows1252) {
    ScanResult result = scan("\x80\x8e\x9f\xff");
    EXPECT_EQ(result.format.encoding, TextEncoding::Windows1252);
}

TEST(TextScan, StrictUtf8RejectsMalformedSequences) {
    const char* const samples[] = {
        "\xc0\x80",         // overlong NUL
        "\xe0\x80\xaf",     // overlong slash
        "\xed\xa0\x80",     // surrogate U+D800
        "\xf4\xa0\x80\x80", // above U+10FFFF
        "\xf5\x80\x80\x80", // invalid lead byte
        "ab\xe2\x82",       // cut off at the end
        "\xe2\x28\xa1",     // continuation byte missing
    };
    for (const char* sample : samples) {
        EXPECT_EQ(scan(sample).format.encoding, TextEncoding::Windows1252);
    }
}

TEST(TextScan, StrictUtf8AcceptsBoundaryCharacters) {
    const char* const samples[] = {
        "\xc2\x80",         // U+0080
        "\xed\x9f\xbf",     // U+D7FF
        "\xee\x80\x80",     // U+E000
        "\xf0\x90\x80\x80", // U+10000
        "\xf4\x8f\xbf\xbf", // U+10FFFF
    };
    for (const char* sample : samples) {
        ScanResult result = scan(sample);
        EXPECT_EQ(result.format.encoding, TextEncoding::Utf8);
        EXPECT_EQ(result.longest_line, 1u);
    }
}

TEST(TextScan, ResultDoesNotDependOnBlockSplits) {
    const std::string samples[] = {
        std::string("\xef\xbb\xbf"
                    "a\r\nb\xc3\xa9\rc\n\r\nlast"),
        std::string("\xff\xfe"
                    "a\0\r\0\n\0\xe9\0\r\0",
                    12),
        std::string("x\r\r\ny\xe9z\x81\r"),
    };
    for (const std::string& bytes : samples) {
        ScanResult whole = scan(bytes);
        for (std::size_t cut = 0; cut <= bytes.size(); ++cut) {
            EXPECT_TRUE(same(whole, scan_blocks({bytes.substr(0, cut), bytes.substr(cut)})));
        }
        std::vector<std::string> single_bytes;
        for (char c : bytes) {
            single_bytes.push_back(std::string(1, c));
        }
        EXPECT_TRUE(same(whole, scan_blocks(single_bytes)));
    }
}

TEST(TextScan, OpenWarningRules) {
    ScanResult result;
    result.longest_line = 100;
    EXPECT_EQ(vermilion::open_warning(result, 100), OpenWarning::None);
    EXPECT_EQ(vermilion::open_warning(result, 99), OpenWarning::LongLine);
    result.binary = true;
    EXPECT_EQ(vermilion::open_warning(result, 99), OpenWarning::Binary);
}

TEST(TextScan, LongLineInLargeBlocks) {
    const std::size_t block_size = 64 * 1024;
    const std::string block(block_size, 'x');
    TextScanner scanner;
    for (int i = 0; i < 48; ++i) {
        scanner.feed(block.data(), block.size());
    }
    scanner.feed("\nshort", 6);
    ScanResult result = scanner.finish();
    EXPECT_EQ(result.longest_line, static_cast<std::uint64_t>(48 * block_size));
    EXPECT_EQ(result.endings.lf, 1u);
    EXPECT_EQ(vermilion::open_warning(result, vermilion::kDefaultLongLineLimit),
              OpenWarning::LongLine);
}

} // namespace
