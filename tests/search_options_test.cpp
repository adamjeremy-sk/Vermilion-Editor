// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search options and the check of a regular expression pattern.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "core/search_options.h"

namespace {

using vermilion::effective_search_text;
using vermilion::pattern_error;
using vermilion::search_enabled;
using vermilion::SearchOptions;

SearchOptions regex_of(const std::string& text, bool match_case = false) {
    SearchOptions options;
    options.text = text;
    options.regex = true;
    options.match_case = match_case;
    return options;
}

TEST(SearchOptions, TheDefaultsAreAnEmptyCaseBlindPlainSearch) {
    SearchOptions options;
    EXPECT_EQ(options.text, std::string());
    EXPECT_FALSE(options.match_case);
    EXPECT_FALSE(options.whole_word);
    EXPECT_FALSE(options.regex);
    EXPECT_FALSE(search_enabled(options));
}

TEST(SearchOptions, ASearchIsEnabledWhenTheTextIsNotEmpty) {
    SearchOptions options;
    options.text = " ";
    EXPECT_TRUE(search_enabled(options));
}

TEST(SearchOptions, TwoOptionSetsAreEqualWhenEveryFieldIs) {
    SearchOptions a;
    a.text = "abc";
    SearchOptions b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    b.match_case = true;
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    b = a;
    b.whole_word = true;
    EXPECT_TRUE(a != b);
    b = a;
    b.regex = true;
    EXPECT_TRUE(a != b);
    b = a;
    b.text = "abd";
    EXPECT_TRUE(a != b);
}

TEST(EffectiveSearchText, APlainSearchIsHandedOverAsTyped) {
    SearchOptions options;
    options.text = "foo|bar";
    EXPECT_EQ(effective_search_text(options), std::string("foo|bar"));
    options.whole_word = true;
    EXPECT_EQ(effective_search_text(options), std::string("foo|bar"));
    options.match_case = true;
    EXPECT_EQ(effective_search_text(options), std::string("foo|bar"));
}

TEST(EffectiveSearchText, ARegularExpressionWithoutWholeWordIsHandedOverAsTyped) {
    EXPECT_EQ(effective_search_text(regex_of("foo|bar")), std::string("foo|bar"));
    EXPECT_EQ(effective_search_text(regex_of("(a)(b)", true)), std::string("(a)(b)"));
}

TEST(EffectiveSearchText, WholeWordWrapsARegularExpressionInANonCapturingGroup) {
    SearchOptions options = regex_of("foo|bar");
    options.whole_word = true;
    EXPECT_EQ(effective_search_text(options), std::string("(?:foo|bar)"));
    options.text = "(b)(a)r";
    EXPECT_EQ(effective_search_text(options), std::string("(?:(b)(a)r)"));
}

TEST(EffectiveSearchText, AnEmptyTextStaysEmpty) {
    SearchOptions options = regex_of("");
    options.whole_word = true;
    EXPECT_EQ(effective_search_text(options), std::string());
}

TEST(PatternError, APlainSearchHasNoPatternToCheck) {
    SearchOptions options;
    options.text = "(";
    EXPECT_FALSE(pattern_error(options).has_value());
    options.text = "[a-";
    EXPECT_FALSE(pattern_error(options).has_value());
}

TEST(PatternError, AnEmptyRegularExpressionIsNotAnError) {
    EXPECT_FALSE(pattern_error(regex_of("")).has_value());
}

TEST(PatternError, AValidPatternHasNoError) {
    EXPECT_FALSE(pattern_error(regex_of("a(b)c")).has_value());
    EXPECT_FALSE(pattern_error(regex_of("^\\s*#include\\b")).has_value());
    EXPECT_FALSE(pattern_error(regex_of("(?<!a)b")).has_value());
    EXPECT_FALSE(pattern_error(regex_of("(?<name>x+)\\k<name>")).has_value());
    EXPECT_FALSE(pattern_error(regex_of("line one\\nline two")).has_value());
}

TEST(PatternError, ABrokenPatternGivesAReason) {
    for (const char* text : {"(", "a)", "[a-", "*", "a{2,1}", "(?<n>a)(?<n>b)", "\\"}) {
        std::optional<std::string> error = pattern_error(regex_of(text));
        EXPECT_TRUE(error.has_value()) << text;
        if (error.has_value()) {
            EXPECT_FALSE(error->empty()) << text;
        }
    }
}

TEST(PatternError, MatchCaseDoesNotChangeWhetherAPatternIsValid) {
    EXPECT_FALSE(pattern_error(regex_of("abc", true)).has_value());
    EXPECT_FALSE(pattern_error(regex_of("abc", false)).has_value());
    EXPECT_TRUE(pattern_error(regex_of("(", true)).has_value());
    EXPECT_TRUE(pattern_error(regex_of("(", false)).has_value());
}

TEST(PatternError, WholeWordDoesNotChangeTheCheck) {
    SearchOptions good = regex_of("a+");
    good.whole_word = true;
    EXPECT_FALSE(pattern_error(good).has_value());
    SearchOptions bad = regex_of("(");
    bad.whole_word = true;
    EXPECT_TRUE(pattern_error(bad).has_value());
}

TEST(PatternError, WholeWordChecksThePatternInsideItsGroupToo) {
    SearchOptions comment = regex_of("(?x) a #note");
    EXPECT_FALSE(pattern_error(comment).has_value());
    comment.whole_word = true;
    EXPECT_TRUE(pattern_error(comment).has_value());
    SearchOptions unbalanced = regex_of("a)|(b");
    EXPECT_TRUE(pattern_error(unbalanced).has_value());
    unbalanced.whole_word = true;
    EXPECT_TRUE(pattern_error(unbalanced).has_value());
}

} // namespace
