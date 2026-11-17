// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for Go to Line: reading the typed text, fitting it to the document, the texts.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/goto_line.h"

namespace {

using vermilion::goto_past_end_message;
using vermilion::goto_range_text;
using vermilion::GotoRequest;
using vermilion::GotoTarget;
using vermilion::parse_goto;
using vermilion::resolve_goto;

GotoRequest request(std::uint64_t line, std::uint64_t column = 0) {
    GotoRequest result;
    result.line = line;
    result.column = column;
    return result;
}

TEST(ParseGoto, ALineNumberAlone) {
    std::optional<GotoRequest> parsed = parse_goto("120");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 120u);
    EXPECT_EQ(parsed->column, 0u);
}

TEST(ParseGoto, ALineAndAColumn) {
    std::optional<GotoRequest> parsed = parse_goto("120:8");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 120u);
    EXPECT_EQ(parsed->column, 8u);
    parsed = parse_goto("1:1");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 1u);
    EXPECT_EQ(parsed->column, 1u);
}

TEST(ParseGoto, SpacesAndTabsAroundTheWholeTextAreIgnored) {
    std::optional<GotoRequest> parsed = parse_goto("  42  ");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 42u);
    parsed = parse_goto("\t7:3 \t");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 7u);
    EXPECT_EQ(parsed->column, 3u);
}

TEST(ParseGoto, LeadingZerosAreAllowed) {
    std::optional<GotoRequest> parsed = parse_goto("007:010");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 7u);
    EXPECT_EQ(parsed->column, 10u);
}

TEST(ParseGoto, ZeroIsNotALineOrAColumn) {
    EXPECT_FALSE(parse_goto("0").has_value());
    EXPECT_FALSE(parse_goto("000").has_value());
    EXPECT_FALSE(parse_goto("0:5").has_value());
    EXPECT_FALSE(parse_goto("5:0").has_value());
}

TEST(ParseGoto, AnythingElseIsRefused) {
    for (const char* text : {"", "   ", "abc", "12a", "a12", "-3", "+3", "1.5", "1,5", ":5", "5:",
                             ":", "1:2:3", "1 2", "12 : 8", "12: 8", "12 :8", "0x10", "1e3"}) {
        EXPECT_FALSE(parse_goto(text).has_value()) << text;
    }
}

TEST(ParseGoto, OnlyAsciiDigitsCount) {
    EXPECT_FALSE(parse_goto("\xEF\xBC\x91").has_value());
    EXPECT_FALSE(parse_goto("5:\xEF\xBC\x91").has_value());
}

TEST(ParseGoto, AVeryLongNumberIsCappedNotOverflowed) {
    std::optional<GotoRequest> parsed = parse_goto("99999999999999999999");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 999999999u);
    parsed = parse_goto("5:99999999999999999999");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->line, 5u);
    EXPECT_EQ(parsed->column, 999999999u);
}

TEST(ResolveGoto, ALineInsideTheDocumentIsKept) {
    GotoTarget target = resolve_goto(request(120, 8), 500);
    EXPECT_EQ(target.line, 120u);
    EXPECT_EQ(target.column, 8u);
    EXPECT_FALSE(target.past_end);
    target = resolve_goto(request(1), 500);
    EXPECT_EQ(target.line, 1u);
    EXPECT_EQ(target.column, 0u);
    EXPECT_FALSE(target.past_end);
}

TEST(ResolveGoto, TheLastLineIsNotPastTheEnd) {
    GotoTarget target = resolve_goto(request(500, 3), 500);
    EXPECT_EQ(target.line, 500u);
    EXPECT_EQ(target.column, 3u);
    EXPECT_FALSE(target.past_end);
}

TEST(ResolveGoto, ALinePastTheEndBecomesTheLastLineWithoutAColumn) {
    GotoTarget target = resolve_goto(request(501, 8), 500);
    EXPECT_EQ(target.line, 500u);
    EXPECT_EQ(target.column, 0u);
    EXPECT_TRUE(target.past_end);
    target = resolve_goto(request(999999999), 12408);
    EXPECT_EQ(target.line, 12408u);
    EXPECT_TRUE(target.past_end);
}

TEST(ResolveGoto, AnEmptyCountIsTreatedAsOneLine) {
    GotoTarget target = resolve_goto(request(1, 4), 0);
    EXPECT_EQ(target.line, 1u);
    EXPECT_EQ(target.column, 4u);
    EXPECT_FALSE(target.past_end);
    target = resolve_goto(request(2), 0);
    EXPECT_EQ(target.line, 1u);
    EXPECT_TRUE(target.past_end);
}

TEST(GotoTexts, TheRangeIsWrittenWithCommas) {
    EXPECT_EQ(goto_range_text(12408), std::string("1 to 12,408"));
    EXPECT_EQ(goto_range_text(500), std::string("1 to 500"));
    EXPECT_EQ(goto_range_text(1), std::string("1 to 1"));
    EXPECT_EQ(goto_range_text(0), std::string("1 to 1"));
}

TEST(GotoTexts, ThePastTheEndMessageNamesBothLines) {
    EXPECT_EQ(goto_past_end_message(request(99999, 8), 12408),
              std::string("Line 99,999 does not exist; went to the last line, 12,408."));
    EXPECT_EQ(goto_past_end_message(request(501), 500),
              std::string("Line 501 does not exist; went to the last line, 500."));
}

} // namespace
