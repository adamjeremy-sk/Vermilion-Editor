// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for inspect_file: the state of a file and its hash, read off the main loop.

#include <gtest/gtest.h>

#include <glib.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "core/file_hash.h"
#include "ui/file_inspector.h"

namespace {

namespace fs = std::filesystem;

using vermilion::ui::FileInspection;
using vermilion::ui::inspect_file;

// Runs inspect_file and waits for the answer on the default main context (at most 10 seconds).
// `calls` is how many times the callback ran, to show that it runs exactly once.
FileInspection inspect(const std::string& path, bool with_hash, int* calls = nullptr) {
    FileInspection answer;
    int count = 0;
    bool done = false;
    inspect_file(path, with_hash, [&](const FileInspection& result) {
        answer = result;
        ++count;
        done = true;
    });
    EXPECT_FALSE(done) << "the callback must not run inside inspect_file";
    gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!done && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    // Let a second call, if there were one, arrive.
    for (int i = 0; i < 5; ++i) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    if (calls != nullptr) {
        *calls = count;
    }
    return answer;
}

class FileInspectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* made = g_dir_make_tmp("vermilion-inspect-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        root_ = made;
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    std::string put(const std::string& name, const std::string& content) {
        std::string path = root_ + "/" + name;
        std::ofstream out(path, std::ios::binary);
        out << content;
        return path;
    }

    std::string root_;
};

TEST_F(FileInspectorTest, AnExistingFileHasItsSizeAndTime) {
    const std::string path = put("a.txt", "hello world");
    int calls = 0;
    FileInspection result = inspect(path, false, &calls);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(result.readable);
    EXPECT_TRUE(result.disk.exists);
    EXPECT_EQ(result.disk.size, 11u);
    EXPECT_GT(result.disk.mtime_us, 0);
    EXPECT_FALSE(result.hash.has_value());
}

TEST_F(FileInspectorTest, TheTimeIsInMicrosecondsSinceTheEpoch) {
    const std::string path = put("a.txt", "x");
    g_autoptr(GDateTime) now = g_date_time_new_now_utc();
    const gint64 now_us = g_date_time_to_unix_usec(now);
    FileInspection result = inspect(path, false);
    ASSERT_TRUE(result.disk.exists);
    EXPECT_LT(result.disk.mtime_us, now_us + 60 * G_USEC_PER_SEC);
    EXPECT_GT(result.disk.mtime_us, now_us - 600 * G_USEC_PER_SEC);
}

TEST_F(FileInspectorTest, TheHashIsOfTheBytes) {
    const std::string content = "the quick brown fox\r\njumps over\n";
    const std::string path = put("a.txt", content);
    FileInspection result = inspect(path, true);
    ASSERT_TRUE(result.readable);
    ASSERT_TRUE(result.hash.has_value());
    EXPECT_EQ(*result.hash, vermilion::hash_bytes(content.data(), content.size()));
}

TEST_F(FileInspectorTest, AnEmptyFileHasTheEmptyHash) {
    const std::string path = put("empty.txt", "");
    FileInspection result = inspect(path, true);
    ASSERT_TRUE(result.disk.exists);
    EXPECT_EQ(result.disk.size, 0u);
    ASSERT_TRUE(result.hash.has_value());
    EXPECT_EQ(*result.hash, vermilion::hash_bytes("", 0));
}

TEST_F(FileInspectorTest, ALargerFileThanOneBlockIsHashedWhole) {
    std::string content(300000, 'x');
    for (size_t i = 0; i < content.size(); i += 17) {
        content[i] = static_cast<char>(i);
    }
    const std::string path = put("big.bin", content);
    FileInspection result = inspect(path, true);
    ASSERT_TRUE(result.hash.has_value());
    EXPECT_EQ(result.disk.size, content.size());
    EXPECT_EQ(*result.hash, vermilion::hash_bytes(content.data(), content.size()));
}

TEST_F(FileInspectorTest, AMissingFileIsReadableButNotThere) {
    FileInspection result = inspect(root_ + "/nothing.txt", true);
    EXPECT_TRUE(result.readable);
    EXPECT_FALSE(result.disk.exists);
    EXPECT_EQ(result.disk.size, 0u);
    EXPECT_EQ(result.disk.mtime_us, 0);
    EXPECT_FALSE(result.hash.has_value());
}

TEST_F(FileInspectorTest, APathThroughAFileIsNotReadable) {
    const std::string blocker = put("blocker", "a plain file");
    FileInspection result = inspect(blocker + "/inside.txt", true);
    EXPECT_FALSE(result.readable);
    EXPECT_FALSE(result.hash.has_value());
}

TEST_F(FileInspectorTest, ADirectoryHasAStateButNoHash) {
    FileInspection result = inspect(root_, true);
    EXPECT_TRUE(result.readable);
    EXPECT_TRUE(result.disk.exists);
    EXPECT_FALSE(result.hash.has_value());
}

TEST_F(FileInspectorTest, TwoInspectionsRunAtTheSameTime) {
    const std::string first = put("one.txt", "one");
    const std::string second = put("two.txt", "second file");
    FileInspection one;
    FileInspection two;
    int done = 0;
    inspect_file(first, true, [&](const FileInspection& r) {
        one = r;
        ++done;
    });
    inspect_file(second, true, [&](const FileInspection& r) {
        two = r;
        ++done;
    });
    gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (done < 2 && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    EXPECT_EQ(done, 2);
    EXPECT_EQ(one.disk.size, 3u);
    EXPECT_EQ(two.disk.size, 11u);
    ASSERT_TRUE(one.hash.has_value());
    ASSERT_TRUE(two.hash.has_value());
    EXPECT_NE(*one.hash, *two.hash);
}

} // namespace
