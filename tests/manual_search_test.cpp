// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search in a manual page: the matches of a search and stepping between them.

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/manual_search.h"
#include "core/search_options.h"

namespace {

using vermilion::find_matches;
using vermilion::MatchStep;
using vermilion::SearchMatch;
using vermilion::SearchOptions;
using vermilion::step_match;

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

using Matches = std::vector<SearchMatch>;

std::string repeat(const std::string& piece, std::size_t times) {
    std::string out;
    for (std::size_t i = 0; i < times; ++i) {
        out += piece;
    }
    return out;
}

TEST(ManualSearchFind, LiteralMatchesInOrderAsCharacterOffsets) {
    EXPECT_EQ(find_matches("one two one", literal("one")), (Matches{{0, 3}, {8, 11}}));
    EXPECT_EQ(find_matches("one two one", literal("two")), (Matches{{4, 7}}));
    EXPECT_TRUE(find_matches("one two one", literal("six")).empty());
}

TEST(ManualSearchFind, NothingIsFoundForAnEmptySearchOrAnEmptyText) {
    EXPECT_TRUE(find_matches("abc", literal("")).empty());
    EXPECT_TRUE(find_matches("", literal("a")).empty());
    EXPECT_TRUE(find_matches("", with("a*", false, false, true)).empty());
}

TEST(ManualSearchFind, ASearchTextWithANulByteFindsNothing) {
    EXPECT_TRUE(find_matches(std::string("a\0b", 3), literal(std::string("a\0b", 3))).empty());
    EXPECT_TRUE(find_matches("ab", literal(std::string("a\0", 2))).empty());
}

TEST(ManualSearchFind, TextThatIsNotUtf8FindsNothing) {
    EXPECT_TRUE(find_matches("a \xFF a", literal("a")).empty());
}

TEST(ManualSearchFind, MatchCaseDecidesWhetherCaseCounts) {
    EXPECT_EQ(find_matches("Foo foo FOO", with("foo", false, false, false)),
              (Matches{{0, 3}, {4, 7}, {8, 11}}));
    EXPECT_EQ(find_matches("Foo foo FOO", with("foo", true, false, false)), (Matches{{4, 7}}));
    EXPECT_EQ(find_matches("Foo foo FOO", with("FOO", true, false, false)), (Matches{{8, 11}}));
}

TEST(ManualSearchFind, CaseIsIgnoredForNonAsciiLettersToo) {
    EXPECT_EQ(find_matches("\xC3\xA9", with("\xC3\x89", false, false, false)), (Matches{{0, 1}}));
    EXPECT_TRUE(find_matches("\xC3\xA9", with("\xC3\x89", true, false, false)).empty());
}

TEST(ManualSearchFind, ALiteralSearchTakesSpecialCharactersAsTheyAre) {
    EXPECT_EQ(find_matches("axb a.b", literal("a.b")), (Matches{{4, 7}}));
    EXPECT_EQ(find_matches("a+b aab", literal("a+b")), (Matches{{0, 3}}));
    EXPECT_EQ(find_matches("x \\d y", literal("\\d")), (Matches{{2, 4}}));
    EXPECT_EQ(find_matches("(a) [b]", literal("(a)")), (Matches{{0, 3}}));
}

TEST(ManualSearchFind, AMatchDoesNotOverlapTheNext) {
    EXPECT_EQ(find_matches("aaaa", literal("aa")), (Matches{{0, 2}, {2, 4}}));
    EXPECT_EQ(find_matches("aaa", literal("aa")), (Matches{{0, 2}}));
}

TEST(ManualSearchFind, WholeWordNeedsNoWordCharacterOnEitherSide) {
    EXPECT_EQ(find_matches("foo foobar barfoo foo_ foo.", with("foo", false, true, false)),
              (Matches{{0, 3}, {23, 26}}));
    EXPECT_EQ(find_matches("a1 1a _a a", with("a", false, true, false)), (Matches{{9, 10}}));
    EXPECT_EQ(find_matches("(foo)", with("foo", false, true, false)), (Matches{{1, 4}}));
    EXPECT_EQ(find_matches("foo", with("foo", false, true, false)), (Matches{{0, 3}}));
}

TEST(ManualSearchFind, ANonAsciiLetterCountsAsAWordCharacterAndOffsetsAreCharacters) {
    EXPECT_EQ(find_matches("\xC3\xA9"
                           "a a",
                           with("a", false, true, false)),
              (Matches{{3, 4}}));
    EXPECT_EQ(find_matches("\xC3\xA9"
                           "a \xC3\xA9"
                           "a",
                           literal("a")),
              (Matches{{1, 2}, {4, 5}}));
    EXPECT_EQ(find_matches("\xE2\x86\x92 x \xE2\x80\xA2 x", literal("x")),
              (Matches{{2, 3}, {6, 7}}));
}

TEST(ManualSearchFind, ARegularExpressionIsUsedAsWritten) {
    EXPECT_EQ(find_matches("cat cot cut", with("c.t", false, false, true)),
              (Matches{{0, 3}, {4, 7}, {8, 11}}));
    EXPECT_EQ(find_matches("cat cot cut", with("c[ao]t", false, false, true)),
              (Matches{{0, 3}, {4, 7}}));
    EXPECT_EQ(find_matches("a1 b22 c333", with("[0-9]+", false, false, true)),
              (Matches{{1, 2}, {4, 6}, {8, 11}}));
}

TEST(ManualSearchFind, ARegularExpressionMatchesAtEveryLineStartAndEnd) {
    EXPECT_EQ(find_matches("one\ntwo\nthree", with("^t", false, false, true)),
              (Matches{{4, 5}, {8, 9}}));
    EXPECT_EQ(find_matches("one\ntwo\nthree", with("e$", false, false, true)),
              (Matches{{2, 3}, {12, 13}}));
    EXPECT_EQ(find_matches("one\ntwo", with("e\nt", false, false, true)), (Matches{{2, 5}}));
}

TEST(ManualSearchFind, WholeWordKeepsAnAlternationInsideTheBoundaries) {
    EXPECT_EQ(find_matches("foobar foo bar", with("foo|bar", false, true, true)),
              (Matches{{7, 10}, {11, 14}}));
    EXPECT_EQ(find_matches("foobar foo bar", with("foo|bar", false, false, true)),
              (Matches{{0, 3}, {3, 6}, {7, 10}, {11, 14}}));
}

TEST(ManualSearchFind, AnInvalidPatternFindsNothing) {
    EXPECT_TRUE(find_matches("(a)", with("(", false, false, true)).empty());
    EXPECT_TRUE(find_matches("(a)", with("[a", false, true, true)).empty());
    EXPECT_EQ(find_matches("(a)", with("(", false, false, false)), (Matches{{0, 1}}));
}

TEST(ManualSearchFind, MatchesOfNoCharactersAreSkipped) {
    EXPECT_EQ(find_matches("axxb", with("x*", false, false, true)), (Matches{{1, 3}}));
    EXPECT_TRUE(find_matches("abc", with("x*", false, false, true)).empty());
}

TEST(ManualSearchFind, AtMostTenThousandMatchesAreReported) {
    const std::string text = repeat("a ", 12000);
    const Matches found = find_matches(text, literal("a"));
    ASSERT_EQ(found.size(), vermilion::kMaxMatches);
    EXPECT_EQ(found.back(), (SearchMatch{2 * (vermilion::kMaxMatches - 1),
                                         2 * (vermilion::kMaxMatches - 1) + 1}));
}

// A plain search to compare with: ASCII only, as the generated texts are.
Matches naive(const std::string& text, const std::string& needle, bool match_case,
              bool whole_word) {
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; };
    const auto word = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               c == '_';
    };
    Matches out;
    std::size_t i = 0;
    while (i + needle.size() <= text.size()) {
        bool same = true;
        for (std::size_t k = 0; k < needle.size() && same; ++k) {
            same = match_case ? text[i + k] == needle[k] : lower(text[i + k]) == lower(needle[k]);
        }
        const bool left = i == 0 || !word(text[i - 1]);
        const bool right = i + needle.size() == text.size() || !word(text[i + needle.size()]);
        if (same && (!whole_word || (left && right))) {
            out.push_back({i, i + needle.size()});
            i += needle.size();
        } else {
            ++i;
        }
    }
    return out;
}

TEST(ManualSearchFind, LiteralSearchAgreesWithAPlainSearchOnGeneratedTexts) {
    std::mt19937 random(12345);
    const std::string alphabet = "abAB_ 1.";
    const std::string needle_alphabet = "abAB_";
    for (int round = 0; round < 3000; ++round) {
        std::string text;
        const std::size_t text_size = random() % 30;
        for (std::size_t i = 0; i < text_size; ++i) {
            text += alphabet[random() % alphabet.size()];
        }
        std::string needle;
        const std::size_t needle_size = 1 + random() % 3;
        for (std::size_t i = 0; i < needle_size; ++i) {
            needle += needle_alphabet[random() % needle_alphabet.size()];
        }
        const bool match_case = (random() & 1) != 0;
        const bool whole_word = (random() & 1) != 0;
        SCOPED_TRACE("text '" + text + "' needle '" + needle + "' case " +
                     std::to_string(match_case) + " word " + std::to_string(whole_word));
        EXPECT_EQ(find_matches(text, with(needle, match_case, whole_word, false)),
                  naive(text, needle, match_case, whole_word));
    }
}

TEST(ManualSearchStep, NoMatchesMeansNoStep) {
    EXPECT_FALSE(step_match(0, std::nullopt, true).has_value());
    EXPECT_FALSE(step_match(0, std::nullopt, false).has_value());
    EXPECT_FALSE(step_match(0, 3, true).has_value());
}

TEST(ManualSearchStep, WithoutACurrentMatchTheFirstOrTheLastIsChosen) {
    const std::optional<MatchStep> first = step_match(5, std::nullopt, true);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->index, 0u);
    EXPECT_FALSE(first->wrapped);
    const std::optional<MatchStep> last = step_match(5, std::nullopt, false);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->index, 4u);
    EXPECT_FALSE(last->wrapped);
}

TEST(ManualSearchStep, ACurrentMatchOutsideTheListCountsAsNone) {
    const std::optional<MatchStep> forward = step_match(3, 7, true);
    ASSERT_TRUE(forward.has_value());
    EXPECT_EQ(forward->index, 0u);
    EXPECT_FALSE(forward->wrapped);
    const std::optional<MatchStep> backward = step_match(3, 3, false);
    ASSERT_TRUE(backward.has_value());
    EXPECT_EQ(backward->index, 2u);
    EXPECT_FALSE(backward->wrapped);
}

TEST(ManualSearchStep, SteppingMovesByOneAndWrapsAtTheEnds) {
    EXPECT_EQ(step_match(5, 2, true)->index, 3u);
    EXPECT_FALSE(step_match(5, 2, true)->wrapped);
    EXPECT_EQ(step_match(5, 2, false)->index, 1u);
    EXPECT_FALSE(step_match(5, 2, false)->wrapped);
    EXPECT_EQ(step_match(5, 4, true)->index, 0u);
    EXPECT_TRUE(step_match(5, 4, true)->wrapped);
    EXPECT_EQ(step_match(5, 0, false)->index, 4u);
    EXPECT_TRUE(step_match(5, 0, false)->wrapped);
    EXPECT_EQ(step_match(5, 3, true)->index, 4u);
    EXPECT_FALSE(step_match(5, 3, true)->wrapped);
    EXPECT_EQ(step_match(5, 1, false)->index, 0u);
    EXPECT_FALSE(step_match(5, 1, false)->wrapped);
}

TEST(ManualSearchStep, ASingleMatchWrapsOntoItself) {
    EXPECT_EQ(step_match(1, 0, true)->index, 0u);
    EXPECT_TRUE(step_match(1, 0, true)->wrapped);
    EXPECT_EQ(step_match(1, 0, false)->index, 0u);
    EXPECT_TRUE(step_match(1, 0, false)->wrapped);
}

} // namespace
