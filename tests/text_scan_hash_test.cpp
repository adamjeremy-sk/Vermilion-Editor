// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for ScanResult::hash: the scanner's content hash against file_hash, for every block split.

#include <gtest/gtest.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "core/file_hash.h"
#include "core/result.h"
#include "core/text_scan.h"

namespace {

using vermilion::FileHash;
using vermilion::hash_bytes;
using vermilion::hash_file_blocking;
using vermilion::Result;
using vermilion::ScanResult;
using vermilion::TextScanner;

// XXH3 64-bit of the empty input and of "abc" (the published reference values).
constexpr std::uint64_t kEmptyHash = 0x2D06800538D394C2ULL;
constexpr std::uint64_t kAbcHash = 0x78AF5F94892F3950ULL;

ScanResult scan(const std::string& bytes) {
    TextScanner scanner;
    scanner.feed(bytes.data(), bytes.size());
    return scanner.finish();
}

std::uint64_t hash_of(const std::string& bytes) {
    return hash_bytes(bytes.data(), bytes.size());
}

// The hash found when the bytes arrive in two blocks cut at `cut`, with an empty block between.
std::uint64_t hash_in_two_blocks(const std::string& bytes, std::size_t cut) {
    TextScanner scanner;
    scanner.feed(bytes.data(), cut);
    scanner.feed(bytes.data(), 0);
    scanner.feed(bytes.data() + cut, bytes.size() - cut);
    return scanner.finish().hash;
}

// One sample per kind of content the scanner tells apart.
std::vector<std::string> samples() {
    std::vector<std::string> all;
    all.push_back("plain ascii\nwith two lines\n");
    all.push_back("crlf lines\r\nsecond\r\nthird\r\n");
    all.push_back("\xEF\xBB\xBF"
                  "utf-8 with a mark \xC3\xA9\xE2\x82\xAC\n");
    all.push_back(std::string("\xFF\xFE"
                              "a\0b\0\n\0",
                              8)); // UTF-16 LE with a byte-order mark
    all.push_back(std::string("binary\0data\0here", 16));
    all.push_back("latin-1 \xE9\xE8 and \x81 undefined in cp1252\n");
    std::string long_text;
    for (int i = 0; i < 3000; ++i) {
        long_text.push_back(static_cast<char>('a' + (i * 5 + i / 11) % 26));
        if (i % 61 == 60)
            long_text.push_back('\n');
    }
    all.push_back(long_text);
    return all;
}

TEST(TextScanHash, EmptyInput) {
    ScanResult result = scan("");
    EXPECT_EQ(result.hash, kEmptyHash);
    EXPECT_EQ(result.size, 0u);
}

TEST(TextScanHash, AbcHasTheReferenceValue) {
    ScanResult result = scan("abc");
    EXPECT_EQ(result.hash, kAbcHash);
    EXPECT_EQ(result.size, 3u);
}

TEST(TextScanHash, ByteOrderMarkIsPartOfTheHash) {
    const std::string bytes = "\xEF\xBB\xBF"
                              "abc";
    ScanResult result = scan(bytes);
    EXPECT_EQ(result.size, 6u);
    EXPECT_EQ(result.hash, hash_of(bytes));
    EXPECT_NE(result.hash, kAbcHash);
}

TEST(TextScanHash, EveryTwoBlockSplitGivesTheWholeHash) {
    for (const std::string& bytes : samples()) {
        for (std::size_t cut = 0; cut <= bytes.size(); ++cut) {
            ASSERT_EQ(hash_in_two_blocks(bytes, cut), hash_of(bytes)) << "cut at " << cut;
        }
    }
}

TEST(TextScanHash, ByteByByteGivesTheWholeHash) {
    for (const std::string& bytes : samples()) {
        TextScanner scanner;
        for (char byte : bytes) {
            scanner.feed(&byte, 1);
        }
        ScanResult result = scanner.finish();
        EXPECT_EQ(result.hash, hash_of(bytes));
        EXPECT_EQ(result.size, bytes.size());
    }
}

TEST(TextScanHash, DoesNotDependOnTheVerdict) {
    // The hash is of the bytes as they are on disk, whatever encoding the scanner decides on.
    for (const std::string& bytes : samples()) {
        EXPECT_EQ(scan(bytes).hash, hash_of(bytes));
    }
}

TEST(TextScanHash, OneChangedByteChangesTheHash) {
    std::string a = "some text\nsecond line\n";
    std::string b = a;
    b[3] = 'X';
    EXPECT_NE(scan(a).hash, scan(b).hash);
}

TEST(TextScanHash, EqualsHashFileBlockingOnTheSameFile) {
    g_autofree char* made = g_dir_make_tmp("vermilion-scanhash-XXXXXX", nullptr);
    ASSERT_NE(made, nullptr);
    const std::string folder = made;
    const std::string path = folder + "/mixed.txt";
    std::string content;
    for (int round = 0; round < 40; ++round) {
        for (const std::string& piece : samples()) {
            content += piece;
        }
    }
    {
        std::ofstream out(path, std::ios::binary);
        out << content;
    }
    TextScanner scanner;
    for (std::size_t at = 0; at < content.size(); at += 1000) {
        scanner.feed(content.data() + at, std::min<std::size_t>(1000, content.size() - at));
    }
    ScanResult scanned = scanner.finish();
    Result<FileHash> file = hash_file_blocking(path);
    g_unlink(path.c_str());
    g_rmdir(folder.c_str());
    ASSERT_TRUE(file.is_ok());
    EXPECT_EQ(scanned.hash, file.value().hash);
    EXPECT_EQ(scanned.size, file.value().size);
}

} // namespace
