// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the result rows of a search over the manual: snippets and one row per page.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_results.h"
#include "core/manual_search.h"
#include "core/search_options.h"

namespace {

using vermilion::match_snippet;
using vermilion::PageResult;
using vermilion::search_pages;
using vermilion::SearchMatch;
using vermilion::SearchOptions;
using vermilion::SearchPage;

constexpr const char* kDots = "\xE2\x80\xA6"; // U+2026

SearchOptions literal(const std::string& text) {
    SearchOptions options;
    options.text = text;
    return options;
}

SearchOptions with(const std::string& text, bool match_case, bool whole_word, bool regex) {
    SearchOptions options;
    options.text = text;
    options.match_case = match_case;
    options.whole_word = whole_word;
    options.regex = regex;
    return options;
}

std::string repeat(const std::string& piece, std::size_t times) {
    std::string out;
    for (std::size_t i = 0; i < times; ++i) {
        out += piece;
    }
    return out;
}

TEST(ManualSnippet, AShortLineIsShownWhole) {
    EXPECT_EQ(match_snippet("Open the file", {9, 13}), std::string("Open the file"));
}

TEST(ManualSnippet, AMatchInTheMiddleOfALongLineHasContextOnBothSides) {
    const std::string text = repeat("x", 100) + "NEEDLE" + repeat("y", 100);
    EXPECT_EQ(match_snippet(text, {100, 106}),
              std::string(kDots) + repeat("x", 27) + "NEEDLE" + repeat("y", 27) + kDots);
}

TEST(ManualSnippet, TheContextBeforeAMatchOfOddLengthIsRoundedDown) {
    const std::string text = repeat("x", 100) + "abcde" + repeat("y", 100);
    EXPECT_EQ(match_snippet(text, {100, 105}),
              std::string(kDots) + repeat("x", 27) + "abcde" + repeat("y", 28) + kDots);
}

TEST(ManualSnippet, AWindowOfOnlySpacesInALongLineGivesNothing) {
    const std::string text = repeat(" ", 100) + "x";
    EXPECT_EQ(match_snippet(text, {50, 52}), std::string());
}

TEST(ManualSnippet, AMatchNearTheStartOfTheLineStartsTheWindowAtTheStart) {
    const std::string text = "needle " + repeat("y", 100);
    EXPECT_EQ(match_snippet(text, {0, 6}), "needle " + repeat("y", 53) + kDots);
    const std::string later = repeat("x", 10) + "needle" + repeat("y", 100);
    EXPECT_EQ(match_snippet(later, {10, 16}), repeat("x", 10) + "needle" + repeat("y", 44) + kDots);
}

TEST(ManualSnippet, AMatchNearTheEndOfTheLineEndsTheWindowAtTheEnd) {
    const std::string text = repeat("x", 100) + " needle";
    EXPECT_EQ(match_snippet(text, {101, 107}), std::string(kDots) + repeat("x", 53) + " needle");
}

TEST(ManualSnippet, AWindowThatCutsTheEndOfAShortLineIsMovedLeft) {
    const std::string text = repeat("a", 30) + "needle" + repeat("b", 14);
    EXPECT_EQ(match_snippet(text, {30, 36}), text);
    const std::string longer = repeat("a", 56) + "needle" + repeat("b", 8);
    EXPECT_EQ(match_snippet(longer, {56, 62}),
              std::string(kDots) + repeat("a", 46) + "needle" + repeat("b", 8));
}

TEST(ManualSnippet, OnlyTheLineOfTheMatchIsUsed) {
    EXPECT_EQ(match_snippet("first line\nthe needle here\nlast", {15, 21}),
              std::string("the needle here"));
}

TEST(ManualSnippet, AMatchRunningPastTheEndOfItsLineIsCutThere) {
    EXPECT_EQ(match_snippet("alpha\nbeta", {2, 8}), std::string("alpha"));
}

TEST(ManualSnippet, AMatchThatBeginsAtANewlineGivesNothing) {
    EXPECT_EQ(match_snippet("a\nb", {1, 2}), std::string());
    EXPECT_EQ(match_snippet("a\nb", {1, 3}), std::string());
}

TEST(ManualSnippet, ALongMatchShowsItsFirstSixtyCharacters) {
    const std::string text = repeat("a", 100);
    EXPECT_EQ(match_snippet(text, {10, 90}), std::string(kDots) + repeat("a", 60) + kDots);
    EXPECT_EQ(match_snippet(text, {0, 60}), repeat("a", 60) + kDots);
}

TEST(ManualSnippet, RunsOfSpacesAndTabsBecomeOneSpaceAndTheEndsAreTrimmed) {
    EXPECT_EQ(match_snippet("a   |   needle   |   b", {8, 14}), std::string("a | needle | b"));
    EXPECT_EQ(match_snippet("      needle", {6, 12}), std::string("needle"));
    EXPECT_EQ(match_snippet("a\t\tneedle \t", {3, 9}), std::string("a needle"));
    EXPECT_EQ(match_snippet("      ", {2, 4}), std::string());
}

TEST(ManualSnippet, TheWindowCountsCharactersNotBytes) {
    const std::string text = repeat("\xC3\xA9", 100) + "needle" + repeat("\xC3\xA9", 100);
    EXPECT_EQ(match_snippet(text, {100, 106}), std::string(kDots) + repeat("\xC3\xA9", 27) +
                                                   "needle" + repeat("\xC3\xA9", 27) + kDots);
}

TEST(ManualSnippet, AMatchOutsideTheTextGivesNothing) {
    EXPECT_EQ(match_snippet("abc", {2, 4}), std::string());
    EXPECT_EQ(match_snippet("abc", {1, 1}), std::string());
    EXPECT_EQ(match_snippet("abc", {3, 4}), std::string());
    EXPECT_EQ(match_snippet("", {0, 1}), std::string());
}

TEST(ManualResultsPages, OnlyPagesWithAMatchAreListedInTheOrderGiven) {
    const std::vector<SearchPage> pages = {
        {"a.md", "Alpha", "Nothing here"},
        {"b.md", "Beta", "Find the needle here and the needle there"},
        {"c.md", "Gamma", "A needle"}};
    const std::vector<PageResult> results = search_pages(pages, literal("needle"));
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].file, std::string("b.md"));
    EXPECT_EQ(results[0].title, std::string("Beta"));
    EXPECT_EQ(results[0].count, 2u);
    EXPECT_EQ(results[0].snippet, std::string("Find the needle here and the needle there"));
    EXPECT_EQ(results[1].file, std::string("c.md"));
    EXPECT_EQ(results[1].count, 1u);
    EXPECT_EQ(results[1].snippet, std::string("A needle"));
}

TEST(ManualResultsPages, TheSnippetShowsTheFirstMatchOfThePage) {
    const std::vector<SearchPage> pages = {{"a.md", "A", "first x\nthen y\nthen x again"}};
    const std::vector<PageResult> results = search_pages(pages, literal("x"));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].count, 2u);
    EXPECT_EQ(results[0].snippet, std::string("first x"));
}

TEST(ManualResultsPages, TheOptionsAreHonoured) {
    const std::vector<SearchPage> pages = {{"a.md", "A", "Needle"}, {"b.md", "B", "needle"}};
    EXPECT_EQ(search_pages(pages, with("needle", false, false, false)).size(), 2u);
    const std::vector<PageResult> cased = search_pages(pages, with("needle", true, false, false));
    ASSERT_EQ(cased.size(), 1u);
    EXPECT_EQ(cased[0].file, std::string("b.md"));
    EXPECT_EQ(search_pages(pages, with("n.edl", false, false, true)).size(), 2u);
    EXPECT_TRUE(search_pages(pages, with("need", false, true, false)).empty());
}

TEST(ManualResultsPages, NothingForNoPagesAnEmptySearchOrABadPattern) {
    const std::vector<SearchPage> pages = {{"a.md", "A", "needle"}};
    EXPECT_TRUE(search_pages({}, literal("needle")).empty());
    EXPECT_TRUE(search_pages(pages, literal("")).empty());
    EXPECT_TRUE(search_pages(pages, with("(", false, false, true)).empty());
}

} // namespace
