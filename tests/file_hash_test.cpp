// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for file_hash: the stream hasher and hash_file_blocking.

#include <gtest/gtest.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <cstdint>
#include <fstream>
#include <string>

#include "core/file_hash.h"
#include "core/result.h"

namespace {

using vermilion::FileHash;
using vermilion::hash_bytes;
using vermilion::hash_file_blocking;
using vermilion::Result;
using vermilion::StreamHasher;

// XXH3 64-bit, seed 0, of the empty input and of "abc" (the published reference values).
constexpr std::uint64_t kEmptyHash = 0x2D06800538D394C2ULL;
constexpr std::uint64_t kAbcHash = 0x78AF5F94892F3950ULL;

// A sample that is not periodic and is longer than one XXH3 stripe set (5000 bytes).
std::string sample_bytes(std::size_t length) {
    std::string bytes;
    for (std::size_t i = 0; i < length; ++i) {
        bytes.push_back(static_cast<char>((i * 7 + i / 13) % 251));
    }
    return bytes;
}

void put(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

class FileHashFiles : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* made = g_dir_make_tmp("vermilion-hash-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        folder_ = made;
    }

    void TearDown() override {
        if (!folder_.empty())
            g_rmdir(folder_.c_str());
    }

    std::string folder_;
};

TEST(StreamHasher, EmptyInputHasTheReferenceValue) {
    StreamHasher hasher;
    EXPECT_EQ(hasher.digest(), kEmptyHash);
    EXPECT_EQ(hash_bytes("", 0), kEmptyHash);
}

TEST(StreamHasher, AbcHasTheReferenceValue) {
    StreamHasher hasher;
    hasher.update("abc", 3);
    EXPECT_EQ(hasher.digest(), kAbcHash);
    EXPECT_EQ(hash_bytes("abc", 3), kAbcHash);
}

TEST(StreamHasher, EmptyBlocksChangeNothing) {
    StreamHasher hasher;
    hasher.update("", 0);
    hasher.update("abc", 3);
    hasher.update("", 0);
    EXPECT_EQ(hasher.digest(), kAbcHash);
}

TEST(StreamHasher, EverySingleSplitGivesTheOneShotValue) {
    const std::string bytes = sample_bytes(5000);
    const std::uint64_t whole = hash_bytes(bytes.data(), bytes.size());
    for (std::size_t cut = 0; cut <= bytes.size(); cut += 37) {
        StreamHasher hasher;
        hasher.update(bytes.data(), cut);
        hasher.update(bytes.data() + cut, bytes.size() - cut);
        ASSERT_EQ(hasher.digest(), whole) << "cut at " << cut;
    }
}

TEST(StreamHasher, ByteByByteGivesTheOneShotValue) {
    const std::string bytes = sample_bytes(3000);
    StreamHasher hasher;
    for (char byte : bytes) {
        hasher.update(&byte, 1);
    }
    EXPECT_EQ(hasher.digest(), hash_bytes(bytes.data(), bytes.size()));
}

TEST(StreamHasher, DigestDoesNotChangeTheState) {
    const std::string bytes = sample_bytes(1000);
    StreamHasher hasher;
    hasher.update(bytes.data(), 400);
    const std::uint64_t first = hasher.digest();
    EXPECT_EQ(hasher.digest(), first);
    hasher.update(bytes.data() + 400, 600);
    EXPECT_EQ(hasher.digest(), hash_bytes(bytes.data(), bytes.size()));
}

TEST(StreamHasher, DifferentBytesGiveDifferentHashes) {
    std::string a = sample_bytes(1000);
    std::string b = a;
    b[500] = static_cast<char>(b[500] + 1);
    EXPECT_NE(hash_bytes(a.data(), a.size()), hash_bytes(b.data(), b.size()));
}

TEST_F(FileHashFiles, EmptyFile) {
    const std::string path = folder_ + "/empty.bin";
    put(path, "");
    Result<FileHash> result = hash_file_blocking(path);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size, 0u);
    EXPECT_EQ(result.value().hash, kEmptyHash);
    g_unlink(path.c_str());
}

TEST_F(FileHashFiles, SmallFile) {
    const std::string path = folder_ + "/abc.txt";
    put(path, "abc");
    Result<FileHash> result = hash_file_blocking(path);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size, 3u);
    EXPECT_EQ(result.value().hash, kAbcHash);
    g_unlink(path.c_str());
}

TEST_F(FileHashFiles, FileLargerThanOneBlock) {
    // 200,000 bytes: three full blocks of 65,536 and a short one.
    const std::string bytes = sample_bytes(200000);
    const std::string path = folder_ + "/big.bin";
    put(path, bytes);
    Result<FileHash> result = hash_file_blocking(path);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size, 200000u);
    EXPECT_EQ(result.value().hash, hash_bytes(bytes.data(), bytes.size()));
    g_unlink(path.c_str());
}

TEST_F(FileHashFiles, FileOfExactlyOneBlock) {
    const std::string bytes = sample_bytes(65536);
    const std::string path = folder_ + "/block.bin";
    put(path, bytes);
    Result<FileHash> result = hash_file_blocking(path);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size, 65536u);
    EXPECT_EQ(result.value().hash, hash_bytes(bytes.data(), bytes.size()));
    g_unlink(path.c_str());
}

TEST_F(FileHashFiles, MissingFileIsAnErrorNamingThePath) {
    const std::string path = folder_ + "/missing.txt";
    Result<FileHash> result = hash_file_blocking(path);
    ASSERT_FALSE(result.is_ok());
    EXPECT_NE(result.error().message.find(path), std::string::npos);
}

TEST_F(FileHashFiles, DirectoryIsAnError) {
    Result<FileHash> result = hash_file_blocking(folder_);
    ASSERT_FALSE(result.is_ok());
    EXPECT_NE(result.error().message.find(folder_), std::string::npos);
}

} // namespace
