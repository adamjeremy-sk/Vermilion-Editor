// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the contents tree of the real manual (docs/manual/*.md) holds every topic.

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/manual_outline.h"
#include "core/manual_page.h"
#include "core/manual_text.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

using vermilion::ManualText;

// Every *.md file of docs/manual, parsed and laid out, by file name.
std::map<std::string, ManualText> read_pages() {
    std::map<std::string, ManualText> pages;
    std::error_code error;
    for (fs::directory_iterator it(VERMILION_MANUAL_DIR, error), end; !error && it != end;
         it.increment(error)) {
        if (it->path().extension() != ".md") {
            continue;
        }
        std::ifstream in(it->path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        pages[it->path().filename().string()] = vermilion::build_manual_text(
            vermilion::parse_manual(text.str()), vermilion::kManualColumns);
    }
    return pages;
}

TEST(ManualOutlineFiles, EveryTopicIsInTheTreeOnceAndTheHomePageIsFirst) {
    const std::map<std::string, ManualText> pages = read_pages();
    ASSERT_FALSE(pages.empty());
    const std::vector<vermilion::OutlineTopic> outline =
        vermilion::build_outline("index.md", pages);
    ASSERT_EQ(outline.size(), pages.size());
    EXPECT_EQ(outline.front().file, std::string("index.md"));
    std::set<std::string> seen;
    for (const vermilion::OutlineTopic& topic : outline) {
        EXPECT_TRUE(seen.insert(topic.file).second) << topic.file << " is listed twice";
        EXPECT_EQ(pages.count(topic.file), 1u) << topic.file;
    }
}

TEST(ManualOutlineFiles, TheTreeFollowsTheLinksOfTheIndexPage) {
    const std::map<std::string, ManualText> pages = read_pages();
    const std::vector<std::string> linked = vermilion::linked_topics(pages.at("index.md"));
    const std::vector<vermilion::OutlineTopic> outline =
        vermilion::build_outline("index.md", pages);
    ASSERT_EQ(outline.size(), linked.size() + 1);
    for (std::size_t i = 0; i < linked.size(); ++i) {
        EXPECT_EQ(outline[i + 1].file, linked[i]);
    }
}

TEST(ManualOutlineFiles, EveryTopicHasATitleOfItsOwnAndItsChildrenPointAtTheirHeadings) {
    const std::map<std::string, ManualText> pages = read_pages();
    std::set<std::string> titles;
    for (const vermilion::OutlineTopic& topic : vermilion::build_outline("index.md", pages)) {
        SCOPED_TRACE(topic.file);
        const ManualText& page = pages.at(topic.file);
        EXPECT_FALSE(topic.title.empty());
        EXPECT_NE(topic.title, topic.file) << "the page has no level-1 heading";
        EXPECT_TRUE(titles.insert(topic.title).second) << "title used twice: " << topic.title;
        EXPECT_EQ(topic.headings.size() + 1, page.headings.size());
        for (const vermilion::OutlineHeading& child : topic.headings) {
            ASSERT_LT(child.index, page.headings.size());
            EXPECT_EQ(page.headings[child.index].title, child.title);
            EXPECT_EQ(page.headings[child.index].level, child.level);
        }
    }
}

} // namespace
