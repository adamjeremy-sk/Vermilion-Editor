// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for open_arguments: what a path is, the typed answer and the texts.

#include <gtest/gtest.h>

#include <sys/stat.h>

#include <filesystem>
#include <string>

#include "core/open_arguments.h"
#include "open_test_dir.h"

namespace {

namespace fs = std::filesystem;
using open_test::OpenTestDir;
using vermilion::Answer;
using vermilion::classify_path;
using vermilion::PathKind;

TEST_F(OpenTestDir, AMissingNameIsMissing) {
    EXPECT_EQ(classify_path(path_of("nothing.txt")), PathKind::Missing);
}

TEST_F(OpenTestDir, AFileIsAFile) {
    EXPECT_EQ(classify_path(make_file("a.txt")), PathKind::File);
}

TEST_F(OpenTestDir, AFolderIsADirectory) {
    EXPECT_EQ(classify_path(make_dir("sub")), PathKind::Directory);
}

TEST_F(OpenTestDir, ALinkIsJudgedByItsTarget) {
    std::string file = make_file("a.txt");
    std::string dir = make_dir("sub");
    std::error_code ec;
    fs::create_symlink(file, root_ / "link_to_file", ec);
    fs::create_symlink(dir, root_ / "link_to_dir", ec);
    fs::create_symlink(path_of("gone.txt"), root_ / "dangling", ec);
    EXPECT_EQ(classify_path(path_of("link_to_file")), PathKind::File);
    EXPECT_EQ(classify_path(path_of("link_to_dir")), PathKind::Directory);
    EXPECT_EQ(classify_path(path_of("dangling")), PathKind::Missing);
}

TEST_F(OpenTestDir, ALinkLoopIsLeftToTheOpenToReport) {
    std::error_code ec;
    fs::create_symlink("loop", root_ / "loop", ec);
    EXPECT_EQ(classify_path(path_of("loop")), PathKind::File);
}

TEST_F(OpenTestDir, ANameBelowAFileIsMissing) {
    std::string file = make_file("a.txt");
    EXPECT_EQ(classify_path(file + "/inner.txt"), PathKind::Missing);
}

TEST_F(OpenTestDir, APipeIsAnotherKindOfEntry) {
    ASSERT_EQ(::mkfifo(path_of("pipe").c_str(), 0600), 0);
    EXPECT_EQ(classify_path(path_of("pipe")), PathKind::Other);
}

TEST(ParseAnswer, EmptyAndYesMeanYes) {
    EXPECT_EQ(vermilion::parse_answer(""), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("\n"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("  \t\r\n"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("y"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("Y\n"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("yes"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer(" YES \n"), Answer::Yes);
    EXPECT_EQ(vermilion::parse_answer("Yes"), Answer::Yes);
}

TEST(ParseAnswer, NMeansNo) {
    EXPECT_EQ(vermilion::parse_answer("n"), Answer::No);
    EXPECT_EQ(vermilion::parse_answer("N\n"), Answer::No);
    EXPECT_EQ(vermilion::parse_answer("no"), Answer::No);
    EXPECT_EQ(vermilion::parse_answer("\tNo\r\n"), Answer::No);
    EXPECT_EQ(vermilion::parse_answer("NO"), Answer::No);
}

TEST(ParseAnswer, AnythingElseAsksAgain) {
    EXPECT_EQ(vermilion::parse_answer("maybe"), Answer::Ask);
    EXPECT_EQ(vermilion::parse_answer("yy"), Answer::Ask);
    EXPECT_EQ(vermilion::parse_answer("nope"), Answer::Ask);
    EXPECT_EQ(vermilion::parse_answer("y n"), Answer::Ask);
    EXPECT_EQ(vermilion::parse_answer("1"), Answer::Ask);
}

TEST(OpenArgumentsMessages, TheTexts) {
    EXPECT_EQ(vermilion::missing_prompt("notes.txt"),
              std::string("'notes.txt' does not exist. Create it? (Y/n)"));
    EXPECT_EQ(vermilion::directory_message("src"), std::string("src is a directory, not a file"));
    EXPECT_EQ(vermilion::not_regular_message("pipe"), std::string("pipe is not a regular file"));
    EXPECT_EQ(vermilion::missing_no_terminal_message("notes.txt"),
              std::string("notes.txt does not exist"));
    EXPECT_EQ(vermilion::not_local_message("https://example.org/a"),
              std::string("Cannot open https://example.org/a: only files on this computer can be "
                          "opened."));
}

} // namespace
