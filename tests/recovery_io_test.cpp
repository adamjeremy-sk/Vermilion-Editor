// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the start-up question's file helpers (a file's time, a copy), on a real main loop.

#include <gtest/gtest.h>

#include <glib.h>
#include <sys/stat.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "core/recovery_file.h"
#include "core/recovery_store.h"
#include "ui/recovery_io.h"

namespace {

using vermilion::RecoveryHeader;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::ui::CopyRead;

constexpr const char* kCopy = "aaaaaaaa-0000-4000-8000-000000000001";
constexpr const char* kMissing = "bbbbbbbb-0000-4000-8000-000000000002";

// A temporary directory, removed with the object.
struct Folder {
    Folder() {
        g_autofree char* made = g_dir_make_tmp("vermilion-io-XXXXXX", nullptr);
        if (made != nullptr) {
            root = made;
        }
    }
    ~Folder() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    Folder(const Folder&) = delete;
    Folder& operator=(const Folder&) = delete;

    std::string root;
};

// Runs the main loop until done is set, or fails after ten seconds.
void run_until(const bool& done) {
    gint64 limit = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!done && g_get_monotonic_time() < limit) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(500);
    }
}

void put_copy(const Folder& folder, const std::string& text) {
    RecoveryHeader header;
    header.title = "Untitled 1";
    header.copied_us = 1794452733554000;
    RecoveryStore store(folder.root, RecoveryProgram::Light);
    ASSERT_TRUE(store.write(kCopy, header, text).is_ok());
}

// The result of reading kCopy from the folder, on the real main loop.
CopyRead read_copy(const Folder& folder, const std::string& id) {
    RecoveryStore store(folder.root, RecoveryProgram::Light);
    CopyRead got;
    bool done = false;
    vermilion::ui::read_recovery_copy(store, id, [&](CopyRead read) {
        got = std::move(read);
        done = true;
    });
    run_until(done);
    EXPECT_TRUE(done);
    return got;
}

TEST(RecoveryIo, TheTimeOfAFileIsTheOneTheSystemReports) {
    Folder folder;
    std::string path = folder.root + "/a.txt";
    std::ofstream(path) << "x";
    struct stat st {};
    ASSERT_EQ(0, stat(path.c_str(), &st));
    std::int64_t expected =
        static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000 + st.st_mtim.tv_nsec / 1000;

    std::optional<std::int64_t> got;
    bool done = false;
    vermilion::ui::query_file_time(path, [&](std::optional<std::int64_t> time) {
        got = time;
        done = true;
    });
    run_until(done);
    ASSERT_TRUE(done);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(expected, *got);
}

TEST(RecoveryIo, AMissingFileHasNoTime) {
    Folder folder;
    std::optional<std::int64_t> got = 5;
    bool done = false;
    vermilion::ui::query_file_time(folder.root + "/gone.txt",
                                   [&](std::optional<std::int64_t> time) {
                                       got = time;
                                       done = true;
                                   });
    run_until(done);
    ASSERT_TRUE(done);
    EXPECT_FALSE(got.has_value());
}

TEST(RecoveryIo, TheTimeIsNeverReportedInsideTheCall) {
    Folder folder;
    bool called = false;
    vermilion::ui::query_file_time(folder.root,
                                   [&](std::optional<std::int64_t>) { called = true; });
    EXPECT_FALSE(called);
    run_until(called);
    EXPECT_TRUE(called);
}

TEST(RecoveryIo, AValidCopyIsRead) {
    Folder folder;
    put_copy(folder, "hello\nworld\n");
    CopyRead got = read_copy(folder, kCopy);
    ASSERT_TRUE(got.copy.has_value());
    EXPECT_EQ("hello\nworld\n", got.copy->text);
    EXPECT_EQ("Untitled 1", got.copy->header.title);
    EXPECT_TRUE(got.problem.empty());
}

TEST(RecoveryIo, ACopyIsNeverReadInsideTheCall) {
    Folder folder;
    put_copy(folder, "x");
    RecoveryStore store(folder.root, RecoveryProgram::Light);
    bool called = false;
    vermilion::ui::read_recovery_copy(store, kCopy, [&](CopyRead) { called = true; });
    EXPECT_FALSE(called);
    run_until(called);
    EXPECT_TRUE(called);
}

TEST(RecoveryIo, ABodyWithANulByteIsDamaged) {
    Folder folder;
    put_copy(folder, std::string("ab\0cd", 5));
    CopyRead got = read_copy(folder, kCopy);
    EXPECT_FALSE(got.copy.has_value());
    EXPECT_FALSE(got.problem.empty());
}

TEST(RecoveryIo, ABodyThatIsNotUtf8IsDamaged) {
    Folder folder;
    put_copy(folder, "ab\xff\xfe"
                     "cd");
    CopyRead got = read_copy(folder, kCopy);
    EXPECT_FALSE(got.copy.has_value());
    EXPECT_FALSE(got.problem.empty());
}

TEST(RecoveryIo, ACopyThatIsNotThereGivesAProblem) {
    Folder folder;
    put_copy(folder, "x");
    CopyRead got = read_copy(folder, kMissing);
    EXPECT_FALSE(got.copy.has_value());
    EXPECT_FALSE(got.problem.empty());
}

TEST(RecoveryIo, AFileThatIsNotACopyGivesAProblem) {
    Folder folder;
    RecoveryStore store(folder.root, RecoveryProgram::Light);
    ASSERT_EQ(0, g_mkdir_with_parents(store.folder().c_str(), 0700));
    std::ofstream(store.path_of(kCopy)) << "this is not a recovery copy\n";
    CopyRead got = read_copy(folder, kCopy);
    EXPECT_FALSE(got.copy.has_value());
    EXPECT_FALSE(got.problem.empty());
}

TEST(RecoveryIo, AnInvalidIdGivesAProblem) {
    Folder folder;
    CopyRead got = read_copy(folder, "../../etc/passwd");
    EXPECT_FALSE(got.copy.has_value());
    EXPECT_FALSE(got.problem.empty());
}

TEST(RecoveryIo, ALargeCopyIsReadWhole) {
    Folder folder;
    std::string text;
    for (int i = 0; i < 60000; ++i) {
        text += "a line of about fifty characters of ordinary text.\n";
    }
    put_copy(folder, text);
    CopyRead got = read_copy(folder, kCopy);
    ASSERT_TRUE(got.copy.has_value());
    EXPECT_EQ(text.size(), got.copy->text.size());
    EXPECT_TRUE(got.copy->text == text);
}

} // namespace
