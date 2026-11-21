// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of write_if_changed: a file is written only when its content differs.

#include <gtest/gtest.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "core/cache_file.h"

namespace {

namespace fs = std::filesystem;
using vermilion::CacheWrite;
using vermilion::write_if_changed;

class CacheFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* made = g_dir_make_tmp("vermilion-cache-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        root_ = made;
        // Neither level exists yet, like a first start.
        folder_ = root_ + "/one/two";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    std::string path(const std::string& name) const { return folder_ + "/" + name; }

    std::string read(const std::string& name) const {
        std::ifstream in(path(name), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    bool exists(const std::string& full) const {
        std::error_code ec;
        return fs::symlink_status(full, ec).type() != fs::file_type::not_found;
    }

    // The names in the folder, to see that no temporary file stays behind.
    std::vector<std::string> names() const {
        std::vector<std::string> out;
        std::error_code ec;
        for (fs::directory_iterator it(folder_, ec), end; !ec && it != end; it.increment(ec)) {
            out.push_back(it->path().filename().string());
        }
        return out;
    }

    std::string root_;
    std::string folder_;
};

TEST_F(CacheFileTest, AMissingFileIsWrittenAndTheFolderIsMade) {
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "hello"), CacheWrite::Written);
    EXPECT_EQ(read("f.txt"), std::string("hello"));
    struct stat made {};
    ASSERT_EQ(g_stat(folder_.c_str(), &made), 0);
    EXPECT_EQ(made.st_mode & 077, mode_t{0}) << "group and others have no access";
}

TEST_F(CacheFileTest, TheSameBytesAreLeftAlone) {
    ASSERT_EQ(write_if_changed(folder_, "f.txt", "hello"), CacheWrite::Written);
    // An old time stamp: a second write would move it to now.
    const fs::file_time_type old = fs::file_time_type::clock::now() - std::chrono::hours(48);
    std::error_code ec;
    fs::last_write_time(path("f.txt"), old, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "hello"), CacheWrite::Unchanged);
    EXPECT_TRUE(fs::last_write_time(path("f.txt"), ec) == old);
}

TEST_F(CacheFileTest, DifferentBytesOfTheSameLengthAreReplaced) {
    ASSERT_EQ(write_if_changed(folder_, "f.txt", "abc"), CacheWrite::Written);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "abd"), CacheWrite::Written);
    EXPECT_EQ(read("f.txt"), std::string("abd"));
}

TEST_F(CacheFileTest, ALongerOrShorterFileIsReplaced) {
    ASSERT_EQ(write_if_changed(folder_, "f.txt", "abcdef"), CacheWrite::Written);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "abc"), CacheWrite::Written);
    EXPECT_EQ(read("f.txt"), std::string("abc"));
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "abcdefgh"), CacheWrite::Written);
    EXPECT_EQ(read("f.txt"), std::string("abcdefgh"));
}

TEST_F(CacheFileTest, EmptyBytesAreAFileOfSizeZero) {
    EXPECT_EQ(write_if_changed(folder_, "e.txt", ""), CacheWrite::Written);
    EXPECT_TRUE(exists(path("e.txt")));
    EXPECT_EQ(read("e.txt"), std::string(""));
    EXPECT_EQ(write_if_changed(folder_, "e.txt", ""), CacheWrite::Unchanged);
}

TEST_F(CacheFileTest, BytesWithZeroAndHighValuesSurvive) {
    const std::string bytes("a\0b\xff\xfe\r\n\0", 8);
    ASSERT_EQ(bytes.size(), std::size_t{8});
    EXPECT_EQ(write_if_changed(folder_, "b.bin", bytes), CacheWrite::Written);
    EXPECT_EQ(read("b.bin"), bytes);
    EXPECT_EQ(write_if_changed(folder_, "b.bin", bytes), CacheWrite::Unchanged);
}

TEST_F(CacheFileTest, NoTemporaryFileStaysBehind) {
    ASSERT_EQ(write_if_changed(folder_, "f.txt", "one"), CacheWrite::Written);
    ASSERT_EQ(write_if_changed(folder_, "f.txt", "two"), CacheWrite::Written);
    const std::vector<std::string> found = names();
    ASSERT_EQ(found.size(), std::size_t{1});
    EXPECT_EQ(found[0], std::string("f.txt"));
}

TEST_F(CacheFileTest, NamesThatAreNotPlainFileNamesFailAndTouchNothing) {
    for (const char* name : {"", ".", "..", "a/b", "/abs", "x/../y", "dir/"}) {
        SCOPED_TRACE(name);
        EXPECT_EQ(write_if_changed(folder_, name, "x"), CacheWrite::Failed);
    }
    EXPECT_FALSE(exists(root_ + "/one")) << "the folder is not even made";
}

TEST_F(CacheFileTest, AFolderInThePlaceOfTheFileFails) {
    std::error_code ec;
    fs::create_directories(path("f.txt"), ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "x"), CacheWrite::Failed);
    EXPECT_TRUE(fs::is_directory(path("f.txt"), ec));
}

TEST_F(CacheFileTest, AFileInThePlaceOfTheFolderFails) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    {
        std::ofstream blocker(root_ + "/one", std::ios::binary);
        blocker << "not a folder";
    }
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "x"), CacheWrite::Failed);
}

TEST_F(CacheFileTest, ALinkIsReplacedByARegularFileAndItsTargetIsNotWritten) {
    std::error_code ec;
    fs::create_directories(folder_, ec);
    {
        std::ofstream target(path("target.txt"), std::ios::binary);
        target << "x";
    }
    fs::create_symlink("target.txt", path("f.txt"), ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "x"), CacheWrite::Written);
    EXPECT_FALSE(fs::is_symlink(path("f.txt"), ec));
    EXPECT_EQ(read("f.txt"), std::string("x"));
    EXPECT_EQ(read("target.txt"), std::string("x"));
}

// A pipe is replaced without being opened: opening it for reading would wait for a writer.
TEST_F(CacheFileTest, APipeInThePlaceOfTheFileIsReplacedWithoutBlocking) {
    std::error_code ec;
    fs::create_directories(folder_, ec);
    ASSERT_EQ(mkfifo(path("f.txt").c_str(), 0600), 0);
    EXPECT_EQ(write_if_changed(folder_, "f.txt", "x"), CacheWrite::Written);
    EXPECT_TRUE(fs::is_regular_file(path("f.txt"), ec));
    EXPECT_EQ(read("f.txt"), std::string("x"));
}

} // namespace
