// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery folder: listing, reading and deleting copies, and the leftover files.

#include <gtest/gtest.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/recovery_store.h"
#include "recovery_file_test_util.h"
#include "recovery_store_test_util.h"

namespace {

namespace fs = std::filesystem;
using store_test::Names;
using store_test::names_in;
using store_test::put;
using store_test::RecoveryStoreTest;
using store_test::slurp;
using vermilion::RecoveryHeader;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::Result;

#if defined(__SANITIZE_ADDRESS__)
constexpr bool kUnderSanitizer = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
constexpr bool kUnderSanitizer = true;
#else
constexpr bool kUnderSanitizer = false;
#endif
#else
constexpr bool kUnderSanitizer = false;
#endif

// A copy of `total` bytes in all: its header, then a body that is a hole in the file.
void put_sparse_copy(const std::string& path, std::uint64_t total) {
    RecoveryHeader header = recovery_test::untitled_header(total);
    std::size_t head_size = vermilion::encode_header(header).value().size();
    header.length = total - head_size;
    std::string head = vermilion::encode_header(header).value();
    ASSERT_EQ(head.size(), head_size);
    put(path, head);
    fs::resize_file(path, total);
}

// For the life of the object the program may not grow its address space by more than `headroom`.
class AddressSpaceLimit {
public:
    explicit AddressSpaceLimit(rlim_t headroom) {
        getrlimit(RLIMIT_AS, &old_);
        std::ifstream statm("/proc/self/statm");
        unsigned long pages = 0;
        statm >> pages;
        struct rlimit tight = {pages * static_cast<rlim_t>(sysconf(_SC_PAGESIZE)) + headroom,
                               old_.rlim_max};
        setrlimit(RLIMIT_AS, &tight);
    }
    ~AddressSpaceLimit() { setrlimit(RLIMIT_AS, &old_); }

private:
    struct rlimit old_;
};

TEST_F(RecoveryStoreTest, ListWithoutAFolderIsEmpty) {
    Result<std::vector<std::string>> ids = light().list();
    ASSERT_TRUE(ids.is_ok());
    EXPECT_TRUE(ids.value().empty());
    EXPECT_FALSE(fs::exists(folder()));
}

TEST_F(RecoveryStoreTest, ListGivesTheIdsInOrder) {
    for (const char* id : {"b", "a", "c", "a-1"}) {
        ASSERT_TRUE(light().write(id, recovery_test::untitled_header(0), "x").is_ok());
    }
    Result<std::vector<std::string>> ids = light().list();
    ASSERT_TRUE(ids.is_ok());
    EXPECT_EQ(ids.value(), (Names{"a", "a-1", "b", "c"}));
}

TEST_F(RecoveryStoreTest, ListIgnoresEverythingThatIsNotACopy) {
    ASSERT_TRUE(light().write("good", recovery_test::untitled_header(0), "x").is_ok());
    for (const char* name :
         {"notes.txt", "x.vrec.bak", "UPPER.vrec", ".vrec", "a.b.vrec", ".vrec-tmp-AB12"}) {
        put(in_folder(name), "x");
    }
    fs::create_directory(in_folder("dir.vrec"));
    fs::create_symlink(in_folder("good.vrec"), in_folder("link.vrec"));
    Result<std::vector<std::string>> ids = light().list();
    ASSERT_TRUE(ids.is_ok());
    EXPECT_EQ(ids.value(), (Names{"good"}));
}

TEST_F(RecoveryStoreTest, DamagedCopiesAreListedButNeverRead) {
    make_folder();
    std::string good = recovery_test::valid_file();
    put(in_folder("junk.vrec"), "garbage");
    put(in_folder("empty.vrec"), "");
    put(in_folder("short.vrec"), good.substr(0, good.size() - 1));
    put(in_folder("long.vrec"), good + "x");
    put(in_folder("ok.vrec"), good);
    Result<std::vector<std::string>> ids = light().list();
    ASSERT_TRUE(ids.is_ok());
    EXPECT_EQ(ids.value(), (Names{"empty", "junk", "long", "ok", "short"}));
    for (const char* id : {"junk", "empty", "short", "long"}) {
        EXPECT_FALSE(light().read_header(id).is_ok()) << id;
        EXPECT_FALSE(light().read(id).is_ok()) << id;
    }
    EXPECT_TRUE(light().read_header("ok").is_ok());
    ASSERT_TRUE(light().read("ok").is_ok());
    EXPECT_EQ(light().read("ok").value().text, "12345");
}

TEST_F(RecoveryStoreTest, ReadRefusesWhatIsNotARegularFile) {
    make_folder();
    put(in_folder("real.vrec"), recovery_test::valid_file());
    fs::create_symlink(in_folder("real.vrec"), in_folder("link.vrec"));
    fs::create_directory(in_folder("dir.vrec"));
    EXPECT_TRUE(light().read("real").is_ok());
    for (const char* id : {"link", "dir", "none"}) {
        EXPECT_FALSE(light().read_header(id).is_ok()) << id;
        EXPECT_FALSE(light().read(id).is_ok()) << id;
    }
}

TEST_F(RecoveryStoreTest, ReadRefusesAFileOverTheLimitAndAcceptsOneAtTheLimit) {
    make_folder();
    put_sparse_copy(in_folder("limit.vrec"), vermilion::kRecoveryMaxFileBytes);
    put_sparse_copy(in_folder("over.vrec"), vermilion::kRecoveryMaxFileBytes + 1);
    EXPECT_TRUE(light().read_header("limit").is_ok());
    EXPECT_FALSE(light().read_header("over").is_ok());
    EXPECT_FALSE(light().read("over").is_ok());
}

// Reading the header of a copy as large as the limit takes a few kilobytes, not the whole file.
TEST_F(RecoveryStoreTest, ReadHeaderReadsOnlyTheHeader) {
    if (kUnderSanitizer) {
        return; // address space limits do not work with the sanitizers
    }
    make_folder();
    put_sparse_copy(in_folder("limit.vrec"), vermilion::kRecoveryMaxFileBytes);
    bool ok = false;
    {
        AddressSpaceLimit limit(256 * 1024 * 1024);
        ok = light().read_header("limit").is_ok();
    }
    EXPECT_TRUE(ok);
}

TEST_F(RecoveryStoreTest, ReadNeverWaitsOnAPipe) {
    make_folder();
    ASSERT_EQ(mkfifo(in_folder("pipe.vrec").c_str(), 0600), 0);
    EXPECT_FALSE(light().read_header("pipe").is_ok());
    EXPECT_FALSE(light().read("pipe").is_ok());
    Result<std::vector<std::string>> ids = light().list();
    ASSERT_TRUE(ids.is_ok());
    EXPECT_TRUE(ids.value().empty());
}

TEST_F(RecoveryStoreTest, RemoveDeletesTheCopy) {
    for (const char* id : {"a", "b"}) {
        ASSERT_TRUE(light().write(id, recovery_test::untitled_header(0), "x").is_ok());
    }
    EXPECT_TRUE(light().remove("a").is_ok());
    EXPECT_EQ(names_in(folder()), (Names{"b.vrec"}));
    EXPECT_FALSE(light().read("a").is_ok());
    EXPECT_TRUE(light().remove("a").is_ok());
}

TEST_F(RecoveryStoreTest, RemovingACopyOfAFolderThatIsNotThereIsFine) {
    EXPECT_TRUE(light().remove("none").is_ok());
    EXPECT_FALSE(fs::exists(folder()));
}

TEST_F(RecoveryStoreTest, RemoveRefusesABadIdAndTouchesNothing) {
    make_folder();
    put(root_ + "/outside.vrec", "keep");
    put(in_folder("a.vrec"), "keep");
    for (const char* id : {"../outside", "", "A", "a/../a"}) {
        EXPECT_FALSE(light().remove(id).is_ok()) << id;
    }
    EXPECT_TRUE(fs::exists(root_ + "/outside.vrec"));
    EXPECT_TRUE(fs::exists(in_folder("a.vrec")));
}

TEST_F(RecoveryStoreTest, RemoveDeletesALinkNotItsTargetAndLeavesADirectory) {
    make_folder();
    put(root_ + "/target.txt", "TARGET");
    fs::create_symlink(root_ + "/target.txt", in_folder("l.vrec"));
    fs::create_directory(in_folder("d.vrec"));
    EXPECT_TRUE(light().remove("l").is_ok());
    EXPECT_FALSE(light().remove("d").is_ok());
    EXPECT_EQ(slurp(root_ + "/target.txt"), "TARGET");
    EXPECT_EQ(names_in(folder()), (Names{"d.vrec"}));
}

TEST_F(RecoveryStoreTest, LeftoversAreRemovedAndNothingElse) {
    ASSERT_TRUE(light().write("keep", recovery_test::untitled_header(0), "x").is_ok());
    put(in_folder(".vrec-tmp-AAAAAA"), "x");
    put(in_folder(".vrec-tmp-BBBBBB"), "x");
    put(in_folder("notes.txt"), "x");
    put(in_folder(".vrec-tmpX"), "x");
    put(root_ + "/target.txt", "TARGET");
    fs::create_symlink(root_ + "/target.txt", in_folder(".vrec-tmp-LINK"));
    fs::create_directory(in_folder(".vrec-tmp-DIR"));
    EXPECT_EQ(light().remove_leftovers(), std::size_t(3));
    EXPECT_EQ(names_in(folder()), (Names{".vrec-tmp-DIR", ".vrec-tmpX", "keep.vrec", "notes.txt"}));
    EXPECT_EQ(slurp(root_ + "/target.txt"), "TARGET");
    EXPECT_EQ(light().remove_leftovers(), std::size_t(0));
}

TEST_F(RecoveryStoreTest, NoLeftoversWithoutAFolder) {
    EXPECT_EQ(light().remove_leftovers(), std::size_t(0));
    EXPECT_FALSE(fs::exists(folder()));
}

TEST_F(RecoveryStoreTest, TheProgramsKeepSeparateFolders) {
    RecoveryStore ide(root_, RecoveryProgram::Ide);
    ASSERT_TRUE(light().write("x", recovery_test::untitled_header(0), "light text").is_ok());
    ASSERT_TRUE(ide.list().is_ok());
    EXPECT_TRUE(ide.list().value().empty());
    ASSERT_TRUE(ide.write("x", recovery_test::untitled_header(0), "ide text").is_ok());
    EXPECT_EQ(light().read("x").value().text, "light text");
    EXPECT_EQ(ide.read("x").value().text, "ide text");
    EXPECT_TRUE(light().remove("x").is_ok());
    EXPECT_EQ(ide.read("x").value().text, "ide text");
}

} // namespace
