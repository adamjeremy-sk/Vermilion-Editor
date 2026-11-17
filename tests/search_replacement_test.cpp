// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the rules a replacement text must meet: literal, GLib's syntax, groups that exist.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "core/open_messages.h"
#include "core/search_options.h"
#include "core/search_replacement.h"

namespace {

using vermilion::quoted;
using vermilion::replacement_error;
using vermilion::SearchOptions;
using Error = std::optional<std::string>;

SearchOptions regex_of(const std::string& text) {
    SearchOptions options;
    options.text = text;
    options.regex = true;
    return options;
}

// Two groups: number 1 and the named group "n" (number 2).
const char* const kTwoGroups = "(a)(?<n>b)";

Error ok() {
    return std::nullopt;
}

TEST(ReplacementError, ALiteralReplacementIsNeverRefused) {
    SearchOptions options;
    options.text = "(a)";
    for (const char* replacement :
         {"", "plain", "\\q", "end\\", "\\g<", "\\9", "\\10", "&", "$1"}) {
        EXPECT_EQ(replacement_error(options, replacement), ok()) << replacement;
    }
}

TEST(ReplacementError, NothingIsJudgedForAnEmptyOrBrokenPattern) {
    EXPECT_EQ(replacement_error(regex_of(""), "\\3"), ok());
    EXPECT_EQ(replacement_error(regex_of("("), "\\3"), ok());
    EXPECT_EQ(replacement_error(regex_of("("), "\\q"), ok());
}

TEST(ReplacementError, GLibsSyntaxIsAccepted) {
    for (const char* replacement :
         {"",       "plain",  "\\0",   "\\1",       "\\2",         "\\g<0>",    "\\g<1>",
          "\\g<2>", "\\g<n>", "\\n",   "\\t",       "\\r",         "\\a",       "\\f",
          "\\v",    "\\b",    "\\x41", "\\x{263a}", "\\\\",        "\\U\\1\\E", "\\L\\g<n>",
          "\\u\\1", "\\l\\1", "&",     "$1",        "a\\1b\\2c\\0"}) {
        EXPECT_EQ(replacement_error(regex_of(kTwoGroups), replacement), ok()) << replacement;
    }
}

TEST(ReplacementError, GLibsSyntaxErrorsAreReturnedWithGLibsText) {
    for (const char* replacement :
         {"\\e", "\\q", "\\z", "end\\", "\\g<", "\\g<>", "\\g1", "\\g<1"}) {
        Error error = replacement_error(regex_of(kTwoGroups), replacement);
        EXPECT_TRUE(error.has_value()) << replacement;
        if (error.has_value()) {
            EXPECT_FALSE(error->empty()) << replacement;
        }
    }
}

TEST(ReplacementError, ADigitFollowedByADigitIsAmbiguous) {
    const std::string message =
        "A backslash followed by two or more digits is ambiguous: use \\g<12> for a group, or "
        "\\x41 for a character.";
    for (const char* replacement : {"\\10", "\\12", "\\00", "\\01", "\\123", "x\\18y", "\\007"}) {
        EXPECT_EQ(replacement_error(regex_of(kTwoGroups), replacement), Error(message))
            << replacement;
    }
}

TEST(ReplacementError, ADigitBeforeALetterOrAfterAnEscapedBackslashIsFine) {
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\1a"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\2 3"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\\\12"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\\\\\1"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\x41\\x{10}"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<1>0"), ok());
}

TEST(ReplacementError, AGroupThePatternDoesNotHaveIsRefusedByNumber) {
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\3"),
              Error("The replacement uses group 3, but the pattern has only 2 groups."));
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<3>"),
              Error("The replacement uses group 3, but the pattern has only 2 groups."));
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<12>"),
              Error("The replacement uses group 12, but the pattern has only 2 groups."));
    EXPECT_EQ(replacement_error(regex_of("(a)"), "\\2"),
              Error("The replacement uses group 2, but the pattern has only 1 group."));
    EXPECT_EQ(replacement_error(regex_of("a"), "\\1"),
              Error("The replacement uses group 1, but the pattern has no groups."));
    EXPECT_EQ(replacement_error(regex_of("a"), "\\g<1>"),
              Error("The replacement uses group 1, but the pattern has no groups."));
}

TEST(ReplacementError, TheWholeMatchAlwaysExists) {
    EXPECT_EQ(replacement_error(regex_of("a"), "\\0"), ok());
    EXPECT_EQ(replacement_error(regex_of("a"), "[\\g<0>]"), ok());
}

TEST(ReplacementError, AHugeGroupNumberIsCappedAndRefused) {
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<99999999999999999999>"),
              Error("The replacement uses group 999999999, but the pattern has only 2 groups."));
}

TEST(ReplacementError, ANamedGroupMustExist) {
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<n>"), ok());
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<x>"),
              Error("The replacement names the group " + quoted("x") +
                    ", but the pattern has no group with that name."));
    EXPECT_EQ(replacement_error(regex_of("a"), "\\g<n>"),
              Error("The replacement names the group " + quoted("n") +
                    ", but the pattern has no group with that name."));
}

TEST(ReplacementError, NonCapturingGroupsDoNotCount) {
    EXPECT_EQ(replacement_error(regex_of("(?:a)(b)"), "\\1"), ok());
    EXPECT_EQ(replacement_error(regex_of("(?:a)(b)"), "\\2"),
              Error("The replacement uses group 2, but the pattern has only 1 group."));
}

TEST(ReplacementError, TheFirstProblemFromTheLeftIsTheOneReturned) {
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\3\\g<x>"),
              Error("The replacement uses group 3, but the pattern has only 2 groups."));
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\g<x>\\3"),
              Error("The replacement names the group " + quoted("x") +
                    ", but the pattern has no group with that name."));
    EXPECT_EQ(replacement_error(regex_of(kTwoGroups), "\\3\\10"),
              Error("The replacement uses group 3, but the pattern has only 2 groups."));
}

TEST(ReplacementError, GLibsSyntaxCheckComesBeforeTheGroupCheck) {
    Error error = replacement_error(regex_of(kTwoGroups), "\\3\\e");
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->find("The replacement uses group"), std::string::npos);
}

} // namespace
