// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for what File > Reload from Disk asks: reload_ask() and the texts of the question.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/external_change.h"
#include "core/external_messages.h"

namespace {

using vermilion::dont_ask_again_label;
using vermilion::kLargeFileBytes;
using vermilion::reload_ask;
using vermilion::reload_busy_message;
using vermilion::reload_failed_message;
using vermilion::reload_missing_message;
using vermilion::reload_question;
using vermilion::ReloadAsk;
using vermilion::reloaded_by_hand_encoding_message;
using vermilion::reloaded_by_hand_message;
using vermilion::ReloadQuestion;
using vermilion::settings_save_failed_message;
using vermilion::size_in_megabytes;

TEST(ReloadAskTest, AnEditedDocumentAlwaysAsksToDiscard) {
    EXPECT_EQ(reload_ask(true, 10, true), ReloadAsk::DiscardChanges);
    EXPECT_EQ(reload_ask(true, 10, false), ReloadAsk::DiscardChanges);
    EXPECT_EQ(reload_ask(true, kLargeFileBytes + 1, true), ReloadAsk::DiscardChanges);
    EXPECT_EQ(reload_ask(true, kLargeFileBytes + 1, false), ReloadAsk::DiscardChanges);
}

TEST(ReloadAskTest, ACleanSmallDocumentReloadsAtOnce) {
    EXPECT_EQ(reload_ask(false, 0, true), ReloadAsk::Nothing);
    EXPECT_EQ(reload_ask(false, 4096, true), ReloadAsk::Nothing);
}

TEST(ReloadAskTest, TheLimitItselfStillReloadsAtOnce) {
    EXPECT_EQ(reload_ask(false, kLargeFileBytes, true), ReloadAsk::Nothing);
}

TEST(ReloadAskTest, OneByteOverTheLimitAsksWhenTheSettingIsOn) {
    EXPECT_EQ(reload_ask(false, kLargeFileBytes + 1, true), ReloadAsk::LargeFile);
    EXPECT_EQ(reload_ask(false, kLargeFileBytes * 5, true), ReloadAsk::LargeFile);
}

TEST(ReloadAskTest, ALargeCleanDocumentReloadsAtOnceWhenTheSettingIsOff) {
    EXPECT_EQ(reload_ask(false, kLargeFileBytes + 1, false), ReloadAsk::Nothing);
}

TEST(ReloadQuestionTest, TheDiscardQuestion) {
    const ReloadQuestion question = reload_question("notes.txt", ReloadAsk::DiscardChanges, 123);
    EXPECT_EQ(question.message, std::string("Discard your changes and reload from disk?"));
    EXPECT_EQ(question.detail,
              std::string("Your unsaved changes to \u201cnotes.txt\u201d will be lost."));
    EXPECT_EQ(question.go, std::string("Discard and Reload"));
}

TEST(ReloadQuestionTest, TheLargeFileQuestionNamesTheSize) {
    const ReloadQuestion question =
        reload_question("big.log", ReloadAsk::LargeFile, 12 * 1048576ull);
    EXPECT_EQ(question.message, std::string("Reload \u201cbig.log\u201d from disk?"));
    EXPECT_EQ(question.detail, std::string("It is large (12.0 MB), so this may take a moment."));
    EXPECT_EQ(question.go, std::string("Reload"));
}

TEST(ReloadQuestionTest, NothingToAskGivesEmptyTexts) {
    const ReloadQuestion question = reload_question("a.txt", ReloadAsk::Nothing, 50);
    EXPECT_TRUE(question.message.empty());
    EXPECT_TRUE(question.detail.empty());
    EXPECT_TRUE(question.go.empty());
}

TEST(ReloadQuestionTest, TheCheckBoxLabel) {
    EXPECT_EQ(dont_ask_again_label(), std::string("Do not ask again"));
}

TEST(SizeInMegabytes, OneDecimalRoundedToTheNearestTenth) {
    EXPECT_EQ(size_in_megabytes(0), std::string("0.0 MB"));
    EXPECT_EQ(size_in_megabytes(1572864), std::string("1.5 MB"));
    EXPECT_EQ(size_in_megabytes(10485761), std::string("10.0 MB"));
    EXPECT_EQ(size_in_megabytes(11000000), std::string("10.5 MB"));
    EXPECT_EQ(size_in_megabytes(52428800), std::string("50.0 MB"));
    EXPECT_EQ(size_in_megabytes(1048576 * 3 + 52428), std::string("3.0 MB"));
    EXPECT_EQ(size_in_megabytes(1048576 * 3 + 52429), std::string("3.1 MB"));
}

TEST(ReloadMessages, TheStatusLines) {
    EXPECT_EQ(reload_busy_message("a.txt"),
              std::string("\u201ca.txt\u201d is busy. Try Reload from Disk again in a moment."));
    EXPECT_EQ(reload_missing_message("a.txt"),
              std::string("\u201ca.txt\u201d is not on disk, so it was not reloaded."));
    EXPECT_EQ(reload_failed_message("a.txt"), std::string("Could not reload \u201ca.txt\u201d."));
}

TEST(ReloadMessages, TheLineAfterAReloadByHand) {
    EXPECT_EQ(reloaded_by_hand_message("a.txt"),
              std::string("Reloaded \u201ca.txt\u201d from disk"));
}

TEST(ReloadMessages, TheLineAfterAReloadByHandThatChangedTheEncoding) {
    EXPECT_EQ(
        reloaded_by_hand_encoding_message("a.txt", "Windows-1252"),
        std::string("Reloaded \u201ca.txt\u201d from disk; the encoding is now Windows-1252"));
}

TEST(ReloadMessages, TheSettingsFailureLineCarriesTheError) {
    EXPECT_EQ(settings_save_failed_message("Permission denied"),
              std::string("Could not save the settings: Permission denied"));
}

} // namespace
