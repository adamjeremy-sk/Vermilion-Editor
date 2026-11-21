// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that every page of the real manual (docs/manual/*.md) lays out well as Help page text.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_text.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

using vermilion::ManualBlockKind;
using vermilion::ManualText;
using vermilion::SpanKind;
using vermilion::TextRange;

struct Topic {
    std::string name;
    vermilion::ManualPage page;
    ManualText built;
};

// Every *.md file of docs/manual, sorted by name, parsed and laid out.
std::vector<Topic> read_topics() {
    std::vector<Topic> topics;
    std::error_code error;
    for (fs::directory_iterator it(VERMILION_MANUAL_DIR, error), end; !error && it != end;
         it.increment(error)) {
        if (it->path().extension() != ".md") {
            continue;
        }
        std::ifstream in(it->path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        Topic topic;
        topic.name = it->path().filename().string();
        topic.page = vermilion::parse_manual(text.str());
        topic.built = vermilion::build_manual_text(topic.page);
        topics.push_back(topic);
    }
    std::sort(topics.begin(), topics.end(),
              [](const Topic& a, const Topic& b) { return a.name < b.name; });
    return topics;
}

// The characters of `text` from offset `begin` up to but not including `end`.
std::string slice(const std::string& text, std::size_t begin, std::size_t end) {
    std::string out;
    std::size_t number = 0; // the 1-based number of the character the byte belongs to
    for (const char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++number;
        }
        if (number > begin && number <= end) {
            out += c;
        }
    }
    return out;
}

std::size_t count_blocks(const vermilion::ManualPage& page, ManualBlockKind kind) {
    return static_cast<std::size_t>(
        std::count_if(page.blocks.begin(), page.blocks.end(),
                      [kind](const vermilion::ManualBlock& block) { return block.kind == kind; }));
}

// True if the ranges are in order, not empty, do not overlap and end inside the text.
bool ordered(const std::vector<TextRange>& ranges, std::size_t length) {
    std::size_t previous_end = 0;
    for (const TextRange& range : ranges) {
        if (range.begin >= range.end || range.begin < previous_end || range.end > length) {
            return false;
        }
        previous_end = range.end;
    }
    return true;
}

TEST(ManualTextFiles, EveryPageLaysOutWithItsBlocks) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        const ManualText& built = topic.built;
        EXPECT_EQ(built.length, vermilion::character_count(built.text));
        EXPECT_FALSE(built.text.empty());
        EXPECT_NE(built.text.back(), '\n');
        EXPECT_EQ(built.headings.size(), count_blocks(topic.page, ManualBlockKind::Heading));
        EXPECT_EQ(built.list_items.size(), count_blocks(topic.page, ManualBlockKind::ListItem));
        EXPECT_EQ(built.tables.size(), count_blocks(topic.page, ManualBlockKind::Table));
        EXPECT_EQ(built.table_headers.size(), built.tables.size());
    }
}

TEST(ManualTextFiles, EveryRangeIsInOrderAndInsideTheText) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        const ManualText& built = topic.built;
        EXPECT_TRUE(ordered(built.bold, built.length));
        EXPECT_TRUE(ordered(built.code, built.length));
        EXPECT_TRUE(ordered(built.list_items, built.length));
        EXPECT_TRUE(ordered(built.tables, built.length));
        EXPECT_TRUE(ordered(built.table_headers, built.length));
        std::vector<TextRange> links;
        for (const vermilion::LinkRange& link : built.links) {
            links.push_back({link.begin, link.end});
        }
        EXPECT_TRUE(ordered(links, built.length));
        std::vector<TextRange> headings;
        for (const vermilion::HeadingMark& heading : built.headings) {
            headings.push_back({heading.begin, heading.end});
        }
        EXPECT_TRUE(ordered(headings, built.length));
    }
}

TEST(ManualTextFiles, EveryHeadingRangeHoldsItsTitle) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        for (const vermilion::HeadingMark& heading : topic.built.headings) {
            EXPECT_EQ(slice(topic.built.text, heading.begin, heading.end), heading.title);
            EXPECT_GE(heading.level, 1);
            EXPECT_LE(heading.level, 6);
        }
    }
}

TEST(ManualTextFiles, EveryLinkIsFoundAtItsOwnWords) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        for (const vermilion::LinkRange& link : topic.built.links) {
            const vermilion::LinkRange* found = vermilion::link_at(topic.built, link.begin);
            ASSERT_NE(found, nullptr);
            EXPECT_EQ(found->target, link.target);
            EXPECT_EQ(vermilion::link_at(topic.built, link.end - 1), found);
        }
    }
}

TEST(ManualTextFiles, EveryTableLineFitsThePage) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        for (const TextRange& table : topic.built.tables) {
            const std::string text = slice(topic.built.text, table.begin, table.end);
            std::istringstream lines(text);
            std::string line;
            while (std::getline(lines, line)) {
                EXPECT_LE(vermilion::character_count(line), vermilion::kManualColumns) << line;
            }
        }
    }
}

} // namespace
