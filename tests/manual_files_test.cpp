// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the real manual (docs/manual/*.md) stays inside the Markdown subset and links up.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_spans.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

struct Topic {
    std::string name;
    vermilion::ManualPage page;
};

// Every *.md file of docs/manual, sorted by name, parsed.
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
        topics.push_back({it->path().filename().string(), vermilion::parse_manual(text.str())});
    }
    std::sort(topics.begin(), topics.end(),
              [](const Topic& a, const Topic& b) { return a.name < b.name; });
    return topics;
}

void add_links(const std::vector<vermilion::ManualSpan>& spans, std::set<std::string>& targets) {
    for (const vermilion::ManualSpan& span : spans) {
        if (span.kind == vermilion::SpanKind::Link) {
            targets.insert(span.target);
        }
    }
}

// The topic files a page links to.
std::set<std::string> links_of(const vermilion::ManualPage& page) {
    std::set<std::string> targets;
    for (const vermilion::ManualBlock& block : page.blocks) {
        add_links(block.spans, targets);
        for (const vermilion::ManualRow& row : block.rows) {
            for (const vermilion::ManualCell& cell : row) {
                add_links(cell, targets);
            }
        }
    }
    return targets;
}

TEST(ManualFiles, FolderHasAnIndex) {
    std::vector<Topic> topics = read_topics();
    ASSERT_GE(topics.size(), std::size_t{1});
    bool has_index = false;
    for (const Topic& topic : topics) {
        has_index = has_index || topic.name == "index.md";
    }
    EXPECT_TRUE(has_index);
}

TEST(ManualFiles, EveryFileNameIsATopicName) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        EXPECT_TRUE(vermilion::is_topic_name(topic.name));
    }
}

TEST(ManualFiles, EveryPageHasContentInsideTheSubset) {
    for (const Topic& topic : read_topics()) {
        SCOPED_TRACE(topic.name);
        EXPECT_FALSE(topic.page.blocks.empty());
        EXPECT_EQ(topic.page.fallbacks, std::size_t{0});
    }
}

TEST(ManualFiles, EveryLinkOpensATopic) {
    std::vector<Topic> topics = read_topics();
    std::set<std::string> names;
    for (const Topic& topic : topics) {
        names.insert(topic.name);
    }
    for (const Topic& topic : topics) {
        SCOPED_TRACE(topic.name);
        for (const std::string& target : links_of(topic.page)) {
            SCOPED_TRACE(target);
            EXPECT_TRUE(names.count(target) == 1);
        }
    }
}

TEST(ManualFiles, TheIndexLinksEveryOtherTopic) {
    std::vector<Topic> topics = read_topics();
    std::set<std::string> linked;
    for (const Topic& topic : topics) {
        if (topic.name == "index.md") {
            linked = links_of(topic.page);
        }
    }
    for (const Topic& topic : topics) {
        SCOPED_TRACE(topic.name);
        if (topic.name != "index.md") {
            EXPECT_TRUE(linked.count(topic.name) == 1);
        }
    }
}

} // namespace
