// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for make_open_request and paths_to_open: sorting the arguments and the final list.

#include <gtest/gtest.h>

#include <sys/stat.h>

#include <optional>
#include <string>
#include <vector>

#include "core/open_arguments.h"
#include "open_test_dir.h"

namespace {

using open_test::OpenTestDir;
using vermilion::ArgumentFile;
using vermilion::make_open_request;
using vermilion::OpenRequest;
using vermilion::paths_to_open;
using Strings = std::vector<std::string>;

ArgumentFile arg(const std::string& name, const std::string& path) {
    return ArgumentFile{name, path};
}

TEST_F(OpenTestDir, NoArgumentsGiveAnEmptyRequest) {
    OpenRequest request = make_open_request({});
    EXPECT_TRUE(request.paths.empty());
    EXPECT_TRUE(request.missing.empty());
    EXPECT_TRUE(request.messages.empty());
}

TEST_F(OpenTestDir, FilesKeepTheirOrder) {
    std::string b = make_file("b.txt");
    std::string a = make_file("a.txt");
    OpenRequest request = make_open_request({arg("b.txt", b), arg("a.txt", a)});
    EXPECT_EQ(request.paths, (Strings{b, a}));
    EXPECT_TRUE(request.missing.empty());
    EXPECT_TRUE(request.messages.empty());
}

TEST_F(OpenTestDir, ThePathOfAFileThatIsGivenTwiceCountsOnce) {
    std::string a = make_file("a.txt");
    std::string b = make_file("b.txt");
    OpenRequest request = make_open_request({arg("a.txt", a), arg("b.txt", b), arg("./a.txt", a)});
    EXPECT_EQ(request.paths, (Strings{a, b}));
}

TEST_F(OpenTestDir, AMissingNameIsRememberedWithItsPlace) {
    std::string a = make_file("a.txt");
    std::string b = make_file("b.txt");
    std::string fresh = path_of("fresh.txt");
    OpenRequest request =
        make_open_request({arg("a.txt", a), arg("fresh.txt", fresh), arg("b.txt", b)});
    EXPECT_EQ(request.paths, (Strings{a, b}));
    ASSERT_EQ(request.missing.size(), 1u);
    EXPECT_EQ(request.missing[0].name, std::string("fresh.txt"));
    EXPECT_EQ(request.missing[0].path, fresh);
    EXPECT_EQ(request.missing[0].position, 1u);
    EXPECT_TRUE(request.messages.empty());
}

TEST_F(OpenTestDir, ThePathOfAMissingNameCountsOnce) {
    std::string fresh = path_of("fresh.txt");
    OpenRequest request = make_open_request({arg("fresh.txt", fresh), arg("./fresh.txt", fresh)});
    ASSERT_EQ(request.missing.size(), 1u);
    EXPECT_EQ(request.missing[0].name, std::string("fresh.txt"));
}

TEST_F(OpenTestDir, AFolderIsRefusedWithItsName) {
    std::string dir = make_dir("src");
    std::string a = make_file("a.txt");
    OpenRequest request = make_open_request({arg("src", dir), arg("a.txt", a), arg("src/", dir)});
    EXPECT_EQ(request.paths, (Strings{a}));
    EXPECT_EQ(request.messages, (Strings{"src is a directory, not a file"}));
}

TEST_F(OpenTestDir, APipeIsRefusedAsNotRegular) {
    ASSERT_EQ(::mkfifo(path_of("pipe").c_str(), 0600), 0);
    OpenRequest request = make_open_request({arg("pipe", path_of("pipe"))});
    EXPECT_TRUE(request.paths.empty());
    EXPECT_EQ(request.messages, (Strings{"pipe is not a regular file"}));
}

TEST_F(OpenTestDir, AnArgumentWithoutAPathIsRefusedEachTime) {
    std::string a = make_file("a.txt");
    ArgumentFile web{"https://example.org/a", std::nullopt};
    OpenRequest request = make_open_request({web, arg("a.txt", a), web});
    EXPECT_EQ(request.paths, (Strings{a}));
    EXPECT_EQ(request.messages, (Strings{vermilion::not_local_message("https://example.org/a"),
                                         vermilion::not_local_message("https://example.org/a")}));
}

TEST_F(OpenTestDir, RefusalsKeepTheArgumentOrder) {
    std::string dir = make_dir("src");
    ASSERT_EQ(::mkfifo(path_of("pipe").c_str(), 0600), 0);
    OpenRequest request = make_open_request(
        {arg("pipe", path_of("pipe")), ArgumentFile{"x://y", std::nullopt}, arg("src", dir)});
    EXPECT_EQ(request.messages,
              (Strings{"pipe is not a regular file", vermilion::not_local_message("x://y"),
                       "src is a directory, not a file"}));
}

OpenRequest request_with_two_missing(const std::string& a, const std::string& b) {
    OpenRequest request;
    request.paths = {a, b};
    request.missing.push_back({"m1", "/m1", 0});
    request.missing.push_back({"m2", "/m2", 1});
    request.missing.push_back({"m3", "/m3", 2});
    return request;
}

TEST(PathsToOpen, EveryAnswerYesPutsTheNewFilesBackInPlace) {
    OpenRequest request = request_with_two_missing("/a", "/b");
    EXPECT_EQ(paths_to_open(request, {true, true, true}),
              (Strings{"/m1", "/a", "/m2", "/b", "/m3"}));
}

TEST(PathsToOpen, ANoLeavesTheFileOut) {
    OpenRequest request = request_with_two_missing("/a", "/b");
    EXPECT_EQ(paths_to_open(request, {false, true, false}), (Strings{"/a", "/m2", "/b"}));
    EXPECT_EQ(paths_to_open(request, {false, false, false}), (Strings{"/a", "/b"}));
}

TEST(PathsToOpen, AShortListCountsAsNo) {
    OpenRequest request = request_with_two_missing("/a", "/b");
    EXPECT_EQ(paths_to_open(request, {true}), (Strings{"/m1", "/a", "/b"}));
    EXPECT_EQ(paths_to_open(request, {}), (Strings{"/a", "/b"}));
}

TEST(PathsToOpen, TwoNewFilesInARowKeepTheirOrder) {
    OpenRequest request;
    request.paths = {"/a"};
    request.missing.push_back({"m1", "/m1", 1});
    request.missing.push_back({"m2", "/m2", 1});
    EXPECT_EQ(paths_to_open(request, {true, true}), (Strings{"/a", "/m1", "/m2"}));
}

TEST(PathsToOpen, OnlyNewFilesWork) {
    OpenRequest request;
    request.missing.push_back({"m1", "/m1", 0});
    EXPECT_EQ(paths_to_open(request, {true}), (Strings{"/m1"}));
    EXPECT_TRUE(paths_to_open(request, {false}).empty());
}

} // namespace
