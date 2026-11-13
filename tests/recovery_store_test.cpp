// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery folder: ids, the folder, and writing copies safely.

#include <gtest/gtest.h>

#include <glib.h>
#include <sys/resource.h>

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "core/recovery_store.h"
#include "recovery_file_test_util.h"
#include "recovery_store_test_util.h"

namespace {

namespace fs = std::filesystem;
using store_test::mode_of;
using store_test::Names;
using store_test::names_in;
using store_test::put;
using store_test::RecoveryStoreTest;
using store_test::slurp;
using vermilion::RecoveryCopy;
using vermilion::RecoveryHeader;
using vermilion::RecoveryParse;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::Result;
using vermilion::Status;

// For the life of the object no file this process writes may grow past `bytes`.
class FileSizeLimit {
public:
    explicit FileSizeLimit(rlim_t bytes) {
        getrlimit(RLIMIT_FSIZE, &old_);
        previous_ = std::signal(SIGXFSZ, SIG_IGN);
        struct rlimit tight = {bytes, old_.rlim_max};
        setrlimit(RLIMIT_FSIZE, &tight);
    }
    ~FileSizeLimit() {
        setrlimit(RLIMIT_FSIZE, &old_);
        std::signal(SIGXFSZ, previous_);
    }

private:
    struct rlimit old_;
    void (*previous_)(int);
};

TEST(RecoveryIds, ProgramFolderNames) {
    EXPECT_STREQ(vermilion::recovery_program_name(RecoveryProgram::Light), "light");
    EXPECT_STREQ(vermilion::recovery_program_name(RecoveryProgram::Ide), "ide");
}

TEST(RecoveryIds, TheRootIsUnderTheStateFolder) {
    EXPECT_EQ(vermilion::recovery_root("/home/adam/.local/state"),
              "/home/adam/.local/state/vermilion/recovery");
    EXPECT_EQ(vermilion::recovery_root("/state/"), "/state/vermilion/recovery");
    EXPECT_EQ(vermilion::default_recovery_root(), vermilion::recovery_root(g_get_user_state_dir()));
    EXPECT_TRUE(g_path_is_absolute(vermilion::default_recovery_root().c_str()));
}

TEST(RecoveryIds, NewIdsAreValidAndDifferent) {
    std::string a = vermilion::new_recovery_id();
    std::string b = vermilion::new_recovery_id();
    EXPECT_EQ(a.size(), std::size_t(36));
    EXPECT_TRUE(vermilion::valid_recovery_id(a));
    EXPECT_NE(a, b);
    EXPECT_EQ(a[8], '-');
    EXPECT_EQ(a[14], '4');
}

TEST(RecoveryIds, WhichIdsAreValid) {
    for (const char* id : {"a", "0", "abc-123", "0123456789abcdef-0123456789abcdef"}) {
        EXPECT_TRUE(vermilion::valid_recovery_id(id)) << id;
    }
    EXPECT_TRUE(vermilion::valid_recovery_id(std::string(64, 'a')));
    EXPECT_FALSE(vermilion::valid_recovery_id(std::string(65, 'a')));
    for (const char* id : {"", "A", "a.b", "a/b", "../a", "a b", "a_b", ".", "..", "\xc3\xa9"}) {
        EXPECT_FALSE(vermilion::valid_recovery_id(id)) << id;
    }
    EXPECT_FALSE(vermilion::valid_recovery_id(std::string_view("a\0b", 3)));
}

TEST_F(RecoveryStoreTest, TheFolderIsTheRootPlusTheProgram) {
    EXPECT_EQ(light().folder(), root_ + "/light");
    EXPECT_EQ(RecoveryStore(root_, RecoveryProgram::Ide).folder(), root_ + "/ide");
    EXPECT_EQ(light().path_of("abc"), root_ + "/light/abc.vrec");
    EXPECT_EQ(light().path_of("../x"), "");
    EXPECT_EQ(light().path_of(""), "");
}

TEST_F(RecoveryStoreTest, ARootThatIsNotAbsoluteGivesNoFolder) {
    for (const char* root : {"", "relative/path", "."}) {
        RecoveryStore store(root, RecoveryProgram::Light);
        EXPECT_EQ(store.folder(), "") << root;
        EXPECT_EQ(store.path_of("abc"), "");
        EXPECT_FALSE(store.write("abc", recovery_test::untitled_header(0), "x").is_ok());
        EXPECT_FALSE(store.list().is_ok());
        EXPECT_FALSE(store.read("abc").is_ok());
        EXPECT_FALSE(store.read_header("abc").is_ok());
        EXPECT_FALSE(store.remove("abc").is_ok());
        EXPECT_EQ(store.remove_leftovers(), std::size_t(0));
    }
}

TEST_F(RecoveryStoreTest, ACopyReadsBackAsWritten) {
    RecoveryHeader header = recovery_test::file_header(0);
    std::string text = "first line\nsecond line\n\xe2\x82\xac";
    ASSERT_TRUE(light().write("doc-1", header, text).is_ok());
    Result<RecoveryCopy> copy = light().read("doc-1");
    ASSERT_TRUE(copy.is_ok());
    header.length = text.size();
    recovery_test::expect_same(copy.value().header, header);
    EXPECT_EQ(copy.value().text, text);
}

TEST_F(RecoveryStoreTest, TheLengthComesFromTheText) {
    ASSERT_TRUE(light().write("a", recovery_test::untitled_header(999), "12345").is_ok());
    Result<RecoveryParse> parsed = light().read_header("a");
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().header.length, std::uint64_t(5));
    EXPECT_EQ(parsed.value().header.title, std::optional<std::string>("Untitled 3"));
    EXPECT_EQ(fs::file_size(in_folder("a.vrec")), parsed.value().body_offset + 5);
}

TEST_F(RecoveryStoreTest, AnEmptyTextIsACopy) {
    ASSERT_TRUE(light().write("e", recovery_test::untitled_header(0), "").is_ok());
    Result<RecoveryCopy> copy = light().read("e");
    ASSERT_TRUE(copy.is_ok());
    EXPECT_EQ(copy.value().text, "");
    EXPECT_EQ(copy.value().header.length, std::uint64_t(0));
}

TEST_F(RecoveryStoreTest, ATextWithAnyBytesRoundTrips) {
    std::string text = "\n\nlength: 1\n";
    for (std::size_t i = 0; i < 3000000; ++i) {
        text.push_back(static_cast<char>((i * 131 + i / 7) % 256));
    }
    ASSERT_TRUE(light().write("big", recovery_test::file_header(0), text).is_ok());
    Result<RecoveryCopy> copy = light().read("big");
    ASSERT_TRUE(copy.is_ok());
    EXPECT_TRUE(copy.value().text == text);
    Result<RecoveryParse> parsed = light().read_header("big");
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().header.length, std::uint64_t(text.size()));
}

TEST_F(RecoveryStoreTest, TheFoldersAndTheFileArePrivate) {
    RecoveryStore store(root_ + "/deep/er", RecoveryProgram::Ide);
    ASSERT_TRUE(store.write("p", recovery_test::untitled_header(0), "x").is_ok());
    EXPECT_EQ(mode_of(root_ + "/deep"), 0700u);
    EXPECT_EQ(mode_of(root_ + "/deep/er"), 0700u);
    EXPECT_EQ(mode_of(root_ + "/deep/er/ide"), 0700u);
    EXPECT_EQ(mode_of(store.path_of("p")), 0600u);
}

TEST_F(RecoveryStoreTest, ANewCopyReplacesTheOldOneAndLeavesNoTemporaryFile) {
    for (const char* text : {"one", "two two", "three"}) {
        ASSERT_TRUE(light().write("a", recovery_test::untitled_header(0), text).is_ok());
        ASSERT_TRUE(light().write("b", recovery_test::untitled_header(0), text).is_ok());
    }
    EXPECT_EQ(light().read("a").value().text, "three");
    EXPECT_EQ(names_in(folder()), (Names{"a.vrec", "b.vrec"}));
}

TEST_F(RecoveryStoreTest, AFileOfAnotherModeIsReplacedByAPrivateOne) {
    make_folder();
    put(in_folder("a.vrec"), "old");
    chmod(in_folder("a.vrec").c_str(), 0644);
    ASSERT_TRUE(light().write("a", recovery_test::untitled_header(0), "new").is_ok());
    EXPECT_EQ(mode_of(in_folder("a.vrec")), 0600u);
    EXPECT_EQ(light().read("a").value().text, "new");
}

TEST_F(RecoveryStoreTest, ALinkAtTheDestinationIsReplacedNotFollowed) {
    make_folder();
    put(root_ + "/target.txt", "TARGET");
    fs::create_symlink(root_ + "/target.txt", in_folder("a.vrec"));
    ASSERT_TRUE(light().write("a", recovery_test::untitled_header(0), "new").is_ok());
    EXPECT_EQ(slurp(root_ + "/target.txt"), "TARGET");
    EXPECT_FALSE(fs::is_symlink(in_folder("a.vrec")));
    EXPECT_EQ(light().read("a").value().text, "new");
}

TEST_F(RecoveryStoreTest, WriteRefusesABadIdAndCreatesNothing) {
    for (const char* id : {"", "../x", "a/b", "A", "a.b"}) {
        EXPECT_FALSE(light().write(id, recovery_test::untitled_header(0), "x").is_ok()) << id;
    }
    EXPECT_FALSE(fs::exists(folder()));
    EXPECT_FALSE(fs::exists(root_ + "/x.vrec"));
}

TEST_F(RecoveryStoreTest, WriteRefusesAHeaderThatCannotBeWritten) {
    ASSERT_TRUE(light().write("a", recovery_test::untitled_header(0), "good").is_ok());
    RecoveryHeader bad;
    bad.copied_us = 1;
    EXPECT_FALSE(light().write("a", bad, "new").is_ok());
    EXPECT_FALSE(light().write("b", bad, "new").is_ok());
    EXPECT_EQ(light().read("a").value().text, "good");
    EXPECT_EQ(names_in(folder()), (Names{"a.vrec"}));
}

TEST_F(RecoveryStoreTest, WriteFailsWhenTheFolderCannotBeMade) {
    put(root_ + "/light", "I am a file");
    EXPECT_FALSE(light().write("a", recovery_test::untitled_header(0), "x").is_ok());
    EXPECT_FALSE(light().list().is_ok());
    EXPECT_EQ(light().remove_leftovers(), std::size_t(0));
    EXPECT_EQ(slurp(root_ + "/light"), "I am a file");
}

TEST_F(RecoveryStoreTest, AFailedWriteLeavesTheOldCopyAndNoTemporaryFile) {
    ASSERT_TRUE(light().write("a", recovery_test::untitled_header(0), "old text").is_ok());
    std::string big(200000, 'x');
    std::optional<Status> over_old;
    std::optional<Status> over_new;
    {
        FileSizeLimit limit(4096);
        over_old = light().write("a", recovery_test::untitled_header(0), big);
        over_new = light().write("b", recovery_test::untitled_header(0), big);
    }
    ASSERT_TRUE(over_old.has_value() && over_new.has_value());
    EXPECT_FALSE(over_old->is_ok());
    EXPECT_FALSE(over_new->is_ok());
    EXPECT_FALSE(over_old->error().message.empty());
    EXPECT_EQ(light().read("a").value().text, "old text");
    EXPECT_EQ(names_in(folder()), (Names{"a.vrec"}));
    EXPECT_EQ(light().remove_leftovers(), std::size_t(0));
}

TEST_F(RecoveryStoreTest, AFailureWhileMovingTheCopyIntoPlaceLeavesNoTemporaryFile) {
    make_folder();
    fs::create_directory(in_folder("a.vrec"));
    EXPECT_FALSE(light().write("a", recovery_test::untitled_header(0), "x").is_ok());
    EXPECT_EQ(names_in(folder()), (Names{"a.vrec"}));
}

} // namespace
