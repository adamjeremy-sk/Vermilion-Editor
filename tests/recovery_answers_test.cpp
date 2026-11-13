// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the answers of the start-up question: buttons, the text check and the status lines.

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/open_messages.h"
#include "core/recovery_answers.h"
#include "core/recovery_file.h"
#include "core/recovery_offer.h"

namespace {

using vermilion::RecoveryAnswer;
using vermilion::RecoveryHeader;
using vermilion::RecoveryItem;
using Answers = std::vector<RecoveryAnswer>;
using Problem = std::optional<std::string>;

constexpr const char* kNul = "the text contains a NUL character";
constexpr const char* kNotUtf8 = "the text is not valid UTF-8";

RecoveryItem valid_item() {
    RecoveryItem item;
    item.id = "aaaaaaaa-0000-4000-8000-000000000001";
    item.header.uri = "file:///home/adam/notes/todo.txt";
    item.header.copied_us = 1794452733554000;
    return item;
}

RecoveryItem damaged_item() {
    RecoveryItem item;
    item.id = "bbbbbbbb-0000-4000-8000-000000000002";
    item.damaged = true;
    item.problem = "the file is too short";
    return item;
}

TEST(RecoveryAnswers, AValidCopyOffersRecoverDiscardAndDecideLater) {
    EXPECT_TRUE(
        vermilion::recovery_answers(valid_item(), false) ==
        (Answers{RecoveryAnswer::Recover, RecoveryAnswer::Discard, RecoveryAnswer::DecideLater}));
}

TEST(RecoveryAnswers, RecoverAllComesBeforeDecideLaterWhenOffered) {
    EXPECT_TRUE(vermilion::recovery_answers(valid_item(), true) ==
                (Answers{RecoveryAnswer::Recover, RecoveryAnswer::Discard,
                         RecoveryAnswer::RecoverAll, RecoveryAnswer::DecideLater}));
}

TEST(RecoveryAnswers, ADamagedCopyOffersOnlyDiscardAndDecideLater) {
    Answers expected{RecoveryAnswer::Discard, RecoveryAnswer::DecideLater};
    EXPECT_TRUE(vermilion::recovery_answers(damaged_item(), false) == expected);
    EXPECT_TRUE(vermilion::recovery_answers(damaged_item(), true) == expected);
}

TEST(RecoveryAnswers, DecideLaterIsAlwaysTheLastButton) {
    for (bool all : {false, true}) {
        EXPECT_TRUE(vermilion::recovery_answers(valid_item(), all).back() ==
                    RecoveryAnswer::DecideLater);
        EXPECT_TRUE(vermilion::recovery_answers(damaged_item(), all).back() ==
                    RecoveryAnswer::DecideLater);
    }
}

TEST(RecoveryAnswers, EveryButtonHasItsLabel) {
    EXPECT_EQ(std::string(vermilion::recovery_answer_label(RecoveryAnswer::Recover)), "Recover");
    EXPECT_EQ(std::string(vermilion::recovery_answer_label(RecoveryAnswer::Discard)), "Discard");
    EXPECT_EQ(std::string(vermilion::recovery_answer_label(RecoveryAnswer::RecoverAll)),
              "Recover All");
    EXPECT_EQ(std::string(vermilion::recovery_answer_label(RecoveryAnswer::DecideLater)),
              "Decide Later");
}

TEST(RecoveryTextCheck, AcceptsTextThatTheBufferTakes) {
    EXPECT_FALSE(vermilion::recovery_text_problem("").has_value());
    EXPECT_FALSE(vermilion::recovery_text_problem("plain text\nsecond line\r\n").has_value());
    // U+00E9 (2 bytes), U+20AC (3 bytes) and U+1F600 (4 bytes).
    EXPECT_FALSE(vermilion::recovery_text_problem("caf\xC3\xA9").has_value());
    EXPECT_FALSE(vermilion::recovery_text_problem("\xE2\x82\xAC").has_value());
    EXPECT_FALSE(vermilion::recovery_text_problem("\xF0\x9F\x98\x80").has_value());
}

TEST(RecoveryTextCheck, RefusesANulByteWherever) {
    EXPECT_EQ(vermilion::recovery_text_problem(std::string(1, '\0')), Problem(kNul));
    EXPECT_EQ(vermilion::recovery_text_problem(std::string("a\0b", 3)), Problem(kNul));
    EXPECT_EQ(vermilion::recovery_text_problem(std::string("ab\0", 3)), Problem(kNul));
    EXPECT_EQ(vermilion::recovery_text_problem(std::string("\0ab", 3)), Problem(kNul));
}

TEST(RecoveryTextCheck, RefusesTextThatIsNotUtf8) {
    EXPECT_EQ(vermilion::recovery_text_problem("a\x80"), Problem(kNotUtf8));            // lone tail
    EXPECT_EQ(vermilion::recovery_text_problem("\xC0\x80"), Problem(kNotUtf8));         // overlong
    EXPECT_EQ(vermilion::recovery_text_problem("\xE2\x82"), Problem(kNotUtf8));         // cut off
    EXPECT_EQ(vermilion::recovery_text_problem("\xED\xA0\x80"), Problem(kNotUtf8));     // surrogate
    EXPECT_EQ(vermilion::recovery_text_problem("\xF4\x90\x80\x80"), Problem(kNotUtf8)); // > 10FFFF
    EXPECT_EQ(vermilion::recovery_text_problem("\xFF"), Problem(kNotUtf8));
}

TEST(RecoveryTextCheck, LooksOnlyAtTheViewItIsGiven) {
    std::string text = "ab\xFF";
    EXPECT_FALSE(vermilion::recovery_text_problem(std::string_view(text).substr(0, 2)).has_value());
    std::string cut = "caf\xC3\xA9";
    EXPECT_EQ(vermilion::recovery_text_problem(std::string_view(cut).substr(0, 4)),
              Problem(kNotUtf8));
    std::string nul("ab\0cd", 5);
    EXPECT_FALSE(vermilion::recovery_text_problem(std::string_view(nul).substr(0, 2)).has_value());
}

TEST(RecoveryTextCheck, ANulByteIsReportedBeforeBadUtf8) {
    EXPECT_EQ(vermilion::recovery_text_problem(std::string("\xFF\0", 2)), Problem(kNul));
    EXPECT_EQ(vermilion::recovery_text_problem(std::string("\0\xFF", 2)), Problem(kNul));
}

TEST(RecoveryDiscardQuestion, NamesAValidCopysDocument) {
    vermilion::RecoveryQuestion question = vermilion::recovery_discard_question(valid_item());
    EXPECT_EQ(question.message, "Delete this copy?");
    EXPECT_EQ(question.detail, "The recovery copy of " + vermilion::quoted("todo.txt") +
                                   " will be deleted. This cannot be undone.");
}

TEST(RecoveryDiscardQuestion, NamesAnUntitledCopyByItsTitle) {
    RecoveryItem item = valid_item();
    item.header.uri = std::nullopt;
    item.header.title = "Untitled 3";
    EXPECT_EQ(vermilion::recovery_discard_question(item).detail,
              "The recovery copy of " + vermilion::quoted("Untitled 3") +
                  " will be deleted. This cannot be undone.");
}

TEST(RecoveryDiscardQuestion, NamesADamagedCopyByItsFile) {
    vermilion::RecoveryQuestion question = vermilion::recovery_discard_question(damaged_item());
    EXPECT_EQ(question.message, "Delete this copy?");
    EXPECT_EQ(question.detail, "The damaged recovery copy bbbbbbbb-0000-4000-8000-000000000002.vrec"
                               " will be deleted. This cannot be undone.");
}

TEST(RecoveredUntitledMessage, NamesTheFile) {
    EXPECT_EQ(vermilion::recovered_untitled_message(valid_item().header),
              "Recovered the text of " + vermilion::quoted("todo.txt") +
                  " as an untitled document, because that file is already open.");
}

TEST(RecoveryKeptMessage, SaysNothingForNone) {
    EXPECT_EQ(vermilion::recovery_kept_message(0), "");
}

TEST(RecoveryKeptMessage, IsSingularForOneAndPluralForMore) {
    EXPECT_EQ(vermilion::recovery_kept_message(1),
              "1 recovery copy was kept. It is offered again at the next start.");
    EXPECT_EQ(vermilion::recovery_kept_message(2),
              "2 recovery copies were kept. They are offered again at the next start.");
    EXPECT_EQ(vermilion::recovery_kept_message(12),
              "12 recovery copies were kept. They are offered again at the next start.");
}

} // namespace
