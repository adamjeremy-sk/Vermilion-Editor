// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the contents tree of the manual: linked topics, topic rows and the whole tree.

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "core/manual_outline.h"
#include "core/manual_page.h"
#include "core/manual_text.h"
#include "manual_test_columns.h"

namespace {

using vermilion::build_outline;
using vermilion::linked_topics;
using vermilion::make_outline_topic;
using vermilion::ManualText;
using vermilion::OutlineTopic;

ManualText text_of(const std::string& markdown) {
    return vermilion::build_manual_text(vermilion::parse_manual(markdown), kManualTestColumns);
}

TEST(ManualOutline, LinkedTopicsFollowTheOrderOfTheLinks) {
    const ManualText page = text_of("# Home\n- [B](b.md)\n- [A](a.md)\n\nText [C](c.md) here.\n");
    EXPECT_EQ(linked_topics(page), (std::vector<std::string>{"b.md", "a.md", "c.md"}));
}

TEST(ManualOutline, LinkedTopicsListEachFileOnceAtItsFirstLink) {
    const ManualText page = text_of("[A](a.md) [B](b.md) [A again](a.md) [B again](b.md)\n");
    EXPECT_EQ(linked_topics(page), (std::vector<std::string>{"a.md", "b.md"}));
}

TEST(ManualOutline, LinkedTopicsOfAPageWithoutLinksIsEmpty) {
    EXPECT_TRUE(linked_topics(text_of("# Home\nNo links, and `code`.\n")).empty());
    EXPECT_TRUE(linked_topics(text_of("")).empty());
}

TEST(ManualOutline, LinksInATableCountToo) {
    const ManualText page = text_of("| Topic |\n|---|\n| [T](t.md) |\n");
    EXPECT_EQ(linked_topics(page), (std::vector<std::string>{"t.md"}));
}

TEST(ManualOutline, TheFirstLevelOneHeadingIsTheTitleAndNotAChild) {
    const OutlineTopic topic =
        make_outline_topic("x.md", text_of("# The *x* topic\n\n## One\n\ntext\n\n### Two\n"));
    EXPECT_EQ(topic.file, std::string("x.md"));
    EXPECT_EQ(topic.title, std::string("The *x* topic"));
    ASSERT_EQ(topic.headings.size(), 2u);
    EXPECT_EQ(topic.headings[0].title, std::string("One"));
    EXPECT_EQ(topic.headings[0].level, 2);
    EXPECT_EQ(topic.headings[0].index, 1u);
    EXPECT_EQ(topic.headings[1].title, std::string("Two"));
    EXPECT_EQ(topic.headings[1].level, 3);
    EXPECT_EQ(topic.headings[1].index, 2u);
}

TEST(ManualOutline, ATitleMadeOfSeveralSpansIsOnePlainString) {
    const OutlineTopic topic = make_outline_topic("x.md", text_of("# Open **and** `save`\n"));
    EXPECT_EQ(topic.title, std::string("Open and save"));
    EXPECT_TRUE(topic.headings.empty());
}

TEST(ManualOutline, ALaterLevelOneHeadingIsAChild) {
    const OutlineTopic topic =
        make_outline_topic("x.md", text_of("# First\n\n# Second\n\n## Third\n"));
    EXPECT_EQ(topic.title, std::string("First"));
    ASSERT_EQ(topic.headings.size(), 2u);
    EXPECT_EQ(topic.headings[0].title, std::string("Second"));
    EXPECT_EQ(topic.headings[0].level, 1);
    EXPECT_EQ(topic.headings[0].index, 1u);
    EXPECT_EQ(topic.headings[1].title, std::string("Third"));
    EXPECT_EQ(topic.headings[1].index, 2u);
}

TEST(ManualOutline, TheTitleIsTheFirstLevelOneHeadingEvenAfterOtherHeadings) {
    const OutlineTopic topic =
        make_outline_topic("x.md", text_of("## Early\n\n# Title\n\n## Late\n"));
    EXPECT_EQ(topic.title, std::string("Title"));
    ASSERT_EQ(topic.headings.size(), 2u);
    EXPECT_EQ(topic.headings[0].title, std::string("Early"));
    EXPECT_EQ(topic.headings[0].index, 0u);
    EXPECT_EQ(topic.headings[1].title, std::string("Late"));
    EXPECT_EQ(topic.headings[1].index, 2u);
}

TEST(ManualOutline, WithoutALevelOneHeadingTheFileNameIsTheTitleAndAllHeadingsAreChildren) {
    const OutlineTopic topic = make_outline_topic("x.md", text_of("## A\n\n### B\n"));
    EXPECT_EQ(topic.title, std::string("x.md"));
    ASSERT_EQ(topic.headings.size(), 2u);
    EXPECT_EQ(topic.headings[0].index, 0u);
    EXPECT_EQ(topic.headings[1].index, 1u);
    const OutlineTopic bare = make_outline_topic("y.md", text_of("Only a paragraph.\n"));
    EXPECT_EQ(bare.title, std::string("y.md"));
    EXPECT_TRUE(bare.headings.empty());
}

TEST(ManualOutline, TheIndexOfAChildPointsAtItsHeadingMark) {
    const ManualText page = text_of("# T\n\n## A\n\npara\n\n## B\n\n### C\n");
    const OutlineTopic topic = make_outline_topic("x.md", page);
    ASSERT_EQ(topic.headings.size(), 3u);
    for (const vermilion::OutlineHeading& child : topic.headings) {
        ASSERT_LT(child.index, page.headings.size());
        EXPECT_EQ(page.headings[child.index].title, child.title);
        EXPECT_EQ(page.headings[child.index].level, child.level);
    }
}

TEST(ManualOutline, TheTreeStartsWithTheHomePageAndThenFollowsItsLinks) {
    std::map<std::string, ManualText> pages;
    pages["index.md"] = text_of("# Home\n\n- [Beta](beta.md)\n- [Alpha](alpha.md)\n");
    pages["alpha.md"] = text_of("# Alpha\n\n## A1\n");
    pages["beta.md"] = text_of("# Beta\n\n## B1\n\n## B2\n");
    pages["zeta.md"] = text_of("# Zeta\n");
    const std::vector<OutlineTopic> outline = build_outline("index.md", pages);
    ASSERT_EQ(outline.size(), 3u);
    EXPECT_EQ(outline[0].file, std::string("index.md"));
    EXPECT_EQ(outline[0].title, std::string("Home"));
    EXPECT_EQ(outline[1].file, std::string("beta.md"));
    EXPECT_EQ(outline[1].headings.size(), 2u);
    EXPECT_EQ(outline[2].file, std::string("alpha.md"));
    EXPECT_EQ(outline[2].headings.size(), 1u);
}

TEST(ManualOutline, ALinkToAMissingFileOrToTheHomePageIsSkipped) {
    std::map<std::string, ManualText> pages;
    pages["index.md"] =
        text_of("# Home\n\n[Gone](gone.md) [Self](index.md) [A](a.md) [Gone](gone.md)\n");
    pages["a.md"] = text_of("# A\n");
    const std::vector<OutlineTopic> outline = build_outline("index.md", pages);
    ASSERT_EQ(outline.size(), 2u);
    EXPECT_EQ(outline[0].file, std::string("index.md"));
    EXPECT_EQ(outline[1].file, std::string("a.md"));
}

TEST(ManualOutline, TheTreeIsEmptyWithoutTheHomePage) {
    std::map<std::string, ManualText> pages;
    pages["a.md"] = text_of("# A\n");
    EXPECT_TRUE(build_outline("index.md", pages).empty());
    EXPECT_TRUE(build_outline("index.md", {}).empty());
}

TEST(ManualOutline, ALinkedTopicIsListedOnceHoweverOftenTheHomePageLinksToIt) {
    std::map<std::string, ManualText> pages;
    pages["index.md"] = text_of("# Home\n\n[A](a.md) and [A](a.md)\n");
    pages["a.md"] = text_of("# A\n");
    EXPECT_EQ(build_outline("index.md", pages).size(), 2u);
}

} // namespace
