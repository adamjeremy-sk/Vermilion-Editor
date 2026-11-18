// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the texts shown when a file changes on disk.

#include <gtest/gtest.h>

#include <string>

#include "core/external_change.h"
#include "core/external_messages.h"
#include "core/save_messages.h"

namespace {

using vermilion::banner_buttons;
using vermilion::banner_text;
using vermilion::BannerButtons;
using vermilion::BannerKind;
using vermilion::close_question;
using vermilion::close_question_for;
using vermilion::CloseCase;
using vermilion::deleted_message;
using vermilion::quit_changed_line;
using vermilion::reloaded_encoding_message;
using vermilion::reloaded_message;

TEST(ExternalMessages, ReloadedStatusLine) {
    EXPECT_EQ(reloaded_message("notes.txt"),
              std::string("Reloaded \u201cnotes.txt\u201d (changed on disk)"));
}

TEST(ExternalMessages, ReloadedWithAnotherEncodingStatusLine) {
    EXPECT_EQ(reloaded_encoding_message("notes.txt", "Windows-1252"),
              std::string("Reloaded \u201cnotes.txt\u201d (changed on disk); the encoding is now "
                          "Windows-1252"));
}

TEST(ExternalMessages, DeletedStatusLine) {
    EXPECT_EQ(deleted_message("notes.txt"),
              std::string("\u201cnotes.txt\u201d was deleted or moved on disk"));
}

TEST(ExternalMessages, ChangedBannerText) {
    EXPECT_EQ(banner_text(BannerKind::Changed, "a.cpp"),
              std::string("\u201ca.cpp\u201d changed on disk."));
}

TEST(ExternalMessages, ChangedLargeBannerText) {
    EXPECT_EQ(banner_text(BannerKind::ChangedLarge, "big.log"),
              std::string("\u201cbig.log\u201d changed on disk. It is large, so it was not "
                          "reloaded."));
}

TEST(ExternalMessages, ReturnedBannerText) {
    EXPECT_EQ(banner_text(BannerKind::Returned, "a.cpp"),
              std::string("\u201ca.cpp\u201d is back on disk after it was deleted or moved."));
}

TEST(ExternalMessages, NoBannerHasNoText) {
    EXPECT_TRUE(banner_text(BannerKind::None, "a.cpp").empty());
}

TEST(ExternalMessages, ChangedButtons) {
    const BannerButtons buttons = banner_buttons(BannerKind::Changed);
    EXPECT_EQ(buttons.reload, "Reload");
    EXPECT_EQ(buttons.secondary, "Keep mine");
}

TEST(ExternalMessages, ReturnedButtonsAreTheChangedButtons) {
    const BannerButtons buttons = banner_buttons(BannerKind::Returned);
    EXPECT_EQ(buttons.reload, "Reload");
    EXPECT_EQ(buttons.secondary, "Keep mine");
}

TEST(ExternalMessages, ChangedLargeButtons) {
    const BannerButtons buttons = banner_buttons(BannerKind::ChangedLarge);
    EXPECT_EQ(buttons.reload, "Reload");
    EXPECT_EQ(buttons.secondary, "Ignore");
}

TEST(ExternalMessages, NoBannerHasNoButtons) {
    const BannerButtons buttons = banner_buttons(BannerKind::None);
    EXPECT_TRUE(buttons.reload.empty());
    EXPECT_TRUE(buttons.secondary.empty());
}

TEST(ExternalMessages, QuitLine) {
    EXPECT_EQ(quit_changed_line(), std::string("This file also changed on disk."));
}

TEST(ExternalMessages, TheUsualCloseQuestionIsTheSaveMessagesOne) {
    const vermilion::OpenQuestion usual = close_question("notes.txt");
    const vermilion::OpenQuestion question = close_question_for("notes.txt", CloseCase::Edited);
    EXPECT_EQ(question.message, usual.message);
    EXPECT_EQ(question.detail, usual.detail);
}

TEST(ExternalMessages, AChangedFileAddsOneLineToTheDetail) {
    const vermilion::OpenQuestion usual = close_question("notes.txt");
    const vermilion::OpenQuestion question =
        close_question_for("notes.txt", CloseCase::EditedFileChanged);
    EXPECT_EQ(question.message, usual.message);
    EXPECT_EQ(question.detail,
              std::string("If you don't save, your changes will be lost.\nThis file also changed "
                          "on disk."));
}

TEST(ExternalMessages, AGoneFileGetsItsOwnQuestion) {
    const vermilion::OpenQuestion question = close_question_for("notes.txt", CloseCase::FileGone);
    EXPECT_EQ(question.message, std::string("Save \u201cnotes.txt\u201d before closing?"));
    EXPECT_EQ(question.detail,
              std::string("The file was deleted or moved on disk. If you don't save, its text "
                          "will be lost."));
}

} // namespace
