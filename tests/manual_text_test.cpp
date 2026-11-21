// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for manual_text: the text of a page, its ranges, the table merge and link_at.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_text.h"

namespace {

using vermilion::ManualBlock;
using vermilion::ManualBlockKind;
using vermilion::ManualCell;
using vermilion::ManualPage;
using vermilion::ManualRow;
using vermilion::ManualSpan;
using vermilion::ManualText;
using vermilion::SpanKind;
using vermilion::TextRange;

ManualText build(const std::string& markdown) {
    return vermilion::build_manual_text(vermilion::parse_manual(markdown));
}

bool same(const TextRange& range, std::size_t begin, std::size_t end) {
    return range.begin == begin && range.end == end;
}

ManualSpan plain(const std::string& text) {
    return ManualSpan{SpanKind::Text, text, ""};
}

TEST(ManualTextCount, CharactersAreNotBytes) {
    EXPECT_EQ(vermilion::character_count(""), std::size_t{0});
    EXPECT_EQ(vermilion::character_count("abc"), std::size_t{3});
    EXPECT_EQ(vermilion::character_count("caf\xC3\xA9"), std::size_t{4});
    EXPECT_EQ(vermilion::character_count("a\xE2\x80\xA2"
                                         "b"),
              std::size_t{3});
}

TEST(ManualTextPage, AnEmptyPageGivesEmptyText) {
    const ManualText built = vermilion::build_manual_text(ManualPage{});
    EXPECT_EQ(built.text, std::string());
    EXPECT_EQ(built.length, std::size_t{0});
    EXPECT_TRUE(built.headings.empty());
    EXPECT_TRUE(built.tables.empty());
}

TEST(ManualTextPage, BlocksAreOneEmptyLineApartAndItemsOneNewline) {
    const ManualText built =
        build("# Title\nSome **bold** and `code` and [link](a-topic.md).\n\n- one\n- two\n\nEnd");
    EXPECT_EQ(built.text, std::string("Title\n\nSome bold and code and link.\n\n\xE2\x80\xA2 one\n"
                                      "\xE2\x80\xA2 two\n\nEnd"));
    EXPECT_EQ(built.length, std::size_t{53});
    ASSERT_EQ(built.headings.size(), std::size_t{1});
    EXPECT_EQ(built.headings[0].begin, std::size_t{0});
    EXPECT_EQ(built.headings[0].end, std::size_t{5});
    EXPECT_EQ(built.headings[0].level, 1);
    EXPECT_EQ(built.headings[0].title, std::string("Title"));
    ASSERT_EQ(built.bold.size(), std::size_t{1});
    EXPECT_TRUE(same(built.bold[0], 12, 16));
    ASSERT_EQ(built.code.size(), std::size_t{1});
    EXPECT_TRUE(same(built.code[0], 21, 25));
    ASSERT_EQ(built.links.size(), std::size_t{1});
    EXPECT_EQ(built.links[0].begin, std::size_t{30});
    EXPECT_EQ(built.links[0].end, std::size_t{34});
    EXPECT_EQ(built.links[0].target, std::string("a-topic.md"));
    ASSERT_EQ(built.list_items.size(), std::size_t{2});
    EXPECT_TRUE(same(built.list_items[0], 37, 42));
    EXPECT_TRUE(same(built.list_items[1], 43, 48));
}

TEST(ManualTextPage, HeadingsKeepTheirLevelTitleAndSpans) {
    const ManualText built = build("# One\n## Two **x**\n\ntext");
    EXPECT_EQ(built.text, std::string("One\n\nTwo x\n\ntext"));
    ASSERT_EQ(built.headings.size(), std::size_t{2});
    EXPECT_EQ(built.headings[1].level, 2);
    EXPECT_EQ(built.headings[1].title, std::string("Two x"));
    EXPECT_EQ(built.headings[1].begin, std::size_t{5});
    EXPECT_EQ(built.headings[1].end, std::size_t{10});
    ASSERT_EQ(built.bold.size(), std::size_t{1});
    EXPECT_TRUE(same(built.bold[0], 9, 10));
}

TEST(ManualTextPage, AHeadingLevelOutsideOneToSixIsMovedIntoIt) {
    ManualPage page;
    ManualBlock deep;
    deep.kind = ManualBlockKind::Heading;
    deep.level = 9;
    deep.spans.push_back(plain("deep"));
    ManualBlock none;
    none.kind = ManualBlockKind::Heading;
    none.level = 0;
    none.spans.push_back(plain("none"));
    page.blocks = {deep, none};
    const ManualText built = vermilion::build_manual_text(page);
    ASSERT_EQ(built.headings.size(), std::size_t{2});
    EXPECT_EQ(built.headings[0].level, 6);
    EXPECT_EQ(built.headings[1].level, 1);
}

TEST(ManualTextPage, OffsetsCountCharactersNotBytes) {
    const ManualText built = build("Caf\xC3\xA9 **bold** \xE2\x86\x92 [go](a-topic.md)");
    ASSERT_EQ(built.bold.size(), std::size_t{1});
    EXPECT_TRUE(same(built.bold[0], 5, 9));
    ASSERT_EQ(built.links.size(), std::size_t{1});
    EXPECT_EQ(built.links[0].begin, std::size_t{12});
    EXPECT_EQ(built.length, std::size_t{14});
    EXPECT_GT(built.text.size(), built.length);
}

TEST(ManualTextPage, AListFollowsAParagraphAndPrecedesAHeading) {
    const ManualText built = build("para\n\n- a\n- b\n\n## H");
    EXPECT_EQ(built.text, std::string("para\n\n\xE2\x80\xA2 a\n\xE2\x80\xA2 b\n\nH"));
}

TEST(ManualTextPage, EmptyHeadingsAndParagraphsAreSkipped) {
    ManualPage page;
    ManualBlock heading;
    heading.kind = ManualBlockKind::Heading;
    heading.level = 2;
    ManualBlock blank;
    blank.kind = ManualBlockKind::Paragraph;
    blank.spans.push_back(plain(""));
    ManualBlock text;
    text.kind = ManualBlockKind::Paragraph;
    text.spans = {plain("x"), ManualSpan{SpanKind::Bold, "", ""},
                  ManualSpan{SpanKind::Code, "", ""}, ManualSpan{SpanKind::Link, "", "a-topic.md"}};
    ManualBlock item;
    item.kind = ManualBlockKind::ListItem;
    page.blocks = {heading, blank, text, item};
    const ManualText built = vermilion::build_manual_text(page);
    EXPECT_EQ(built.text, std::string("x\n\n\xE2\x80\xA2 "));
    EXPECT_TRUE(built.headings.empty());
    EXPECT_TRUE(built.bold.empty());
    EXPECT_TRUE(built.code.empty());
    EXPECT_TRUE(built.links.empty());
    ASSERT_EQ(built.list_items.size(), std::size_t{1});
    EXPECT_TRUE(same(built.list_items[0], 3, 5));
}

TEST(ManualTextPage, ATableWithoutRowsOrColumnsIsSkipped) {
    ManualBlock before;
    before.kind = ManualBlockKind::Paragraph;
    before.spans.push_back(plain("a"));
    ManualBlock no_rows;
    no_rows.kind = ManualBlockKind::Table;
    no_rows.columns = 2;
    ManualBlock no_columns;
    no_columns.kind = ManualBlockKind::Table;
    no_columns.rows = {ManualRow{}};
    ManualBlock after;
    after.kind = ManualBlockKind::Paragraph;
    after.spans.push_back(plain("b"));
    ManualPage page;
    page.blocks = {before, no_rows, no_columns, after};
    const ManualText built = vermilion::build_manual_text(page);
    EXPECT_EQ(built.text, std::string("a\n\nb"));
    EXPECT_TRUE(built.tables.empty());
    EXPECT_TRUE(built.table_headers.empty());
}

TEST(ManualTextPage, ATableTakesItsColumnCountFromItsLongestRow) {
    ManualBlock table;
    table.kind = ManualBlockKind::Table;
    table.rows = {ManualRow{ManualCell{plain("x")}}};
    ManualPage page;
    page.blocks = {table};
    EXPECT_EQ(vermilion::build_manual_text(page).text, std::string("x\n-"));
}

TEST(ManualTextPage, ATableAfterAParagraphMovesItsRanges) {
    const ManualText built = build("intro\n\n| **A** | B |\n|---|---|\n| [go](a-topic.md) | `c` |");
    EXPECT_EQ(built.text, std::string("intro\n\nA  | B\n---+--\ngo | c"));
    ASSERT_EQ(built.tables.size(), std::size_t{1});
    EXPECT_TRUE(same(built.tables[0], 7, 27));
    ASSERT_EQ(built.table_headers.size(), std::size_t{1});
    EXPECT_TRUE(same(built.table_headers[0], 7, 13));
    ASSERT_EQ(built.bold.size(), std::size_t{1});
    EXPECT_TRUE(same(built.bold[0], 7, 8));
    ASSERT_EQ(built.links.size(), std::size_t{1});
    EXPECT_EQ(built.links[0].begin, std::size_t{21});
    EXPECT_EQ(built.links[0].end, std::size_t{23});
    ASSERT_EQ(built.code.size(), std::size_t{1});
    EXPECT_TRUE(same(built.code[0], 26, 27));
}

TEST(ManualTextPage, EveryRangeInAMixedPageIsInOrder) {
    const ManualText built = build("# A\n\n**x** `y` [z](a-topic.md)\n\n| P | Q |\n|---|---|\n| "
                                   "**b** | `c` |\n\n- **d**\n- `e`");
    const std::vector<std::vector<TextRange>> all = {built.bold, built.code, built.list_items,
                                                     built.tables, built.table_headers};
    for (const std::vector<TextRange>& ranges : all) {
        std::size_t previous_end = 0;
        for (const TextRange& range : ranges) {
            EXPECT_LT(range.begin, range.end);
            EXPECT_GE(range.begin, previous_end);
            EXPECT_LE(range.end, built.length);
            previous_end = range.end;
        }
    }
    EXPECT_EQ(built.bold.size(), std::size_t{3});
    EXPECT_EQ(built.code.size(), std::size_t{3});
}

TEST(ManualTextLink, ALinkCoversItsBeginUpToItsEnd) {
    const ManualText built = build("see [one](a-topic.md) and [two](b-topic.md)");
    ASSERT_EQ(built.links.size(), std::size_t{2});
    EXPECT_EQ(vermilion::link_at(built, 3), nullptr);
    ASSERT_NE(vermilion::link_at(built, 4), nullptr);
    EXPECT_EQ(vermilion::link_at(built, 4)->target, std::string("a-topic.md"));
    EXPECT_EQ(vermilion::link_at(built, 6)->target, std::string("a-topic.md"));
    EXPECT_EQ(vermilion::link_at(built, 7), nullptr);
    EXPECT_EQ(vermilion::link_at(built, 11), nullptr);
    ASSERT_NE(vermilion::link_at(built, 12), nullptr);
    EXPECT_EQ(vermilion::link_at(built, 12)->target, std::string("b-topic.md"));
    EXPECT_EQ(vermilion::link_at(built, 14)->target, std::string("b-topic.md"));
    EXPECT_EQ(vermilion::link_at(built, 15), nullptr);
    EXPECT_EQ(vermilion::link_at(built, 500), nullptr);
}

TEST(ManualTextLink, NoLinksMeansNull) {
    EXPECT_EQ(vermilion::link_at(ManualText{}, 0), nullptr);
}

} // namespace
