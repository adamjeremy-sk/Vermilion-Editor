// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the status line texts of the Find and Replace windows.

#include <gtest/gtest.h>

#include <string>

#include "core/search_messages.h"

namespace {

using vermilion::error_reason;
using vermilion::match_count_message;
using vermilion::pattern_problem_message;
using vermilion::replace_stopped_message;
using vermilion::replaced_message;
using vermilion::replacement_problem_message;
using vermilion::replacing_message;
using vermilion::wrapped_message;

TEST(MatchCountMessage, WhileTheBufferIsBeingScanned) {
    EXPECT_EQ(match_count_message(0, -1), std::string("Searching\u2026"));
    EXPECT_EQ(match_count_message(5, -1), std::string("Searching\u2026"));
}

TEST(MatchCountMessage, NoMatchesIsNotFound) {
    EXPECT_EQ(match_count_message(0, 0), std::string("Not found"));
    EXPECT_EQ(match_count_message(3, 0), std::string("Not found"));
}

TEST(MatchCountMessage, ACurrentMatchGivesItsNumber) {
    EXPECT_EQ(match_count_message(3, 17), std::string("3 of 17 matches"));
    EXPECT_EQ(match_count_message(1, 1), std::string("1 of 1 match"));
    EXPECT_EQ(match_count_message(17, 17), std::string("17 of 17 matches"));
    EXPECT_EQ(match_count_message(1204, 15310), std::string("1,204 of 15,310 matches"));
}

TEST(MatchCountMessage, WithoutACurrentMatchOnlyTheCountIsShown) {
    EXPECT_EQ(match_count_message(0, 17), std::string("17 matches"));
    EXPECT_EQ(match_count_message(-1, 17), std::string("17 matches"));
    EXPECT_EQ(match_count_message(18, 17), std::string("17 matches"));
    EXPECT_EQ(match_count_message(0, 1), std::string("1 match"));
    EXPECT_EQ(match_count_message(0, 15310), std::string("15,310 matches"));
}

TEST(WrappedMessage, NamesTheEndTheSearchCameBackFrom) {
    EXPECT_EQ(wrapped_message(true), std::string("Wrapped to the top"));
    EXPECT_EQ(wrapped_message(false), std::string("Wrapped to the bottom"));
}

TEST(ReplacedMessage, CountsWithTheRightNoun) {
    EXPECT_EQ(replaced_message(0), std::string("Nothing to replace"));
    EXPECT_EQ(replaced_message(1), std::string("Replaced 1 match"));
    EXPECT_EQ(replaced_message(42), std::string("Replaced 42 matches"));
    EXPECT_EQ(replaced_message(1234), std::string("Replaced 1,234 matches"));
}

TEST(ReplacingMessage, ShowsTheKnownTotalAsAnEstimate) {
    EXPECT_EQ(replacing_message(0, 5), std::string("Replacing: 0 of about 5"));
    EXPECT_EQ(replacing_message(1204000, 3400000),
              std::string("Replacing: 1,204,000 of about 3,400,000"));
    EXPECT_EQ(replacing_message(5, 5), std::string("Replacing: 5 of about 5"));
}

TEST(ReplacingMessage, LeavesTheTotalOutWhenItIsUnknownOrTooSmall) {
    EXPECT_EQ(replacing_message(0, 0), std::string("Replacing: 0"));
    EXPECT_EQ(replacing_message(1234, 0), std::string("Replacing: 1,234"));
    EXPECT_EQ(replacing_message(7, 5), std::string("Replacing: 7"));
}

TEST(ReplaceStoppedMessage, CountsWithTheRightNoun) {
    EXPECT_EQ(replace_stopped_message(0), std::string("Stopped, nothing was replaced"));
    EXPECT_EQ(replace_stopped_message(1), std::string("Stopped after replacing 1 match"));
    EXPECT_EQ(replace_stopped_message(4), std::string("Stopped after replacing 4 matches"));
    EXPECT_EQ(replace_stopped_message(1234), std::string("Stopped after replacing 1,234 matches"));
}

TEST(ErrorReason, TakesTheTextAfterAtCharAndTheNumber) {
    EXPECT_EQ(error_reason("Error while compiling regular expression \u2018(\u2019 at char 1: "
                           "missing terminating )"),
              std::string("missing terminating )"));
    EXPECT_EQ(error_reason("Error while compiling regular expression \u2018*\u2019 at char 0: "
                           "nothing to repeat"),
              std::string("nothing to repeat"));
    EXPECT_EQ(error_reason("Error while parsing replacement text \u201c\\e\u201d at char 1: "
                           "unknown escape sequence"),
              std::string("unknown escape sequence"));
}

TEST(ErrorReason, ThePatternItselfMayContainAtChar) {
    EXPECT_EQ(error_reason("Error while compiling regular expression \u2018a at char 3: b(\u2019 "
                           "at char 15: missing terminating )"),
              std::string("missing terminating )"));
}

TEST(ErrorReason, AMessageOfAnotherShapeIsReturnedUnchanged) {
    EXPECT_EQ(error_reason(""), std::string(""));
    EXPECT_EQ(error_reason("The replacement uses group 3, but the pattern has only 2 groups."),
              std::string("The replacement uses group 3, but the pattern has only 2 groups."));
    EXPECT_EQ(error_reason("odd at char x: text"), std::string("odd at char x: text"));
    EXPECT_EQ(error_reason("odd at char 5 text"), std::string("odd at char 5 text"));
    EXPECT_EQ(error_reason("odd at char 5:"), std::string("odd at char 5:"));
}

TEST(ProblemMessages, APatternProblemIsPrefixed) {
    EXPECT_EQ(pattern_problem_message("Error while compiling regular expression \u2018(\u2019 at "
                                      "char 1: missing terminating )"),
              std::string("Not a valid pattern: missing terminating )"));
    EXPECT_EQ(pattern_problem_message("something else"),
              std::string("Not a valid pattern: something else"));
}

TEST(ProblemMessages, AGLibReplacementProblemIsPrefixed) {
    EXPECT_EQ(replacement_problem_message("Error while parsing replacement text \u201c\\e\u201d "
                                          "at char 1: unknown escape sequence"),
              std::string("Not a valid replacement: unknown escape sequence"));
}

TEST(ProblemMessages, OurOwnReplacementSentencesAreKept) {
    const std::string group = "The replacement uses group 3, but the pattern has only 2 groups.";
    EXPECT_EQ(replacement_problem_message(group), group);
    const std::string ambiguous =
        "A backslash followed by two or more digits is ambiguous: use \\g<12> for a group, or "
        "\\x41 for a character.";
    EXPECT_EQ(replacement_problem_message(ambiguous), ambiguous);
}

} // namespace
