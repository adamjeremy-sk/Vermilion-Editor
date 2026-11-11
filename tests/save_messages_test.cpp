// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for save_messages: the close, changed-on-disk and reopen questions and the save texts.

#include <gtest/gtest.h>

#include <string>

#include "core/open_messages.h"
#include "core/save_messages.h"
#include "core/text_format.h"

namespace {

using vermilion::OpenQuestion;

TEST(SaveMessages, CloseQuestion) {
    OpenQuestion question = vermilion::close_question("a.txt");
    EXPECT_EQ(question.message, std::string("Save changes to \u201ca.txt\u201d before closing?"));
    EXPECT_EQ(question.detail, std::string("If you don't save, your changes will be lost."));
}

TEST(SaveMessages, ChangedOnDiskQuestion) {
    OpenQuestion question = vermilion::changed_on_disk_question("a.txt");
    EXPECT_EQ(question.message,
              std::string("\u201ca.txt\u201d has changed on disk since it was opened."));
    EXPECT_EQ(question.detail, std::string("Saving replaces the version on disk with this one."));
}

TEST(SaveMessages, ReopenQuestion) {
    OpenQuestion question = vermilion::reopen_question("a.txt", "Latin-1");
    EXPECT_EQ(question.message, std::string("Reopen \u201ca.txt\u201d as Latin-1?"));
    EXPECT_EQ(question.detail, std::string("Your unsaved changes will be lost."));
}

TEST(SaveMessages, AlreadyOpenError) {
    OpenQuestion error = vermilion::already_open_error("a.txt");
    EXPECT_EQ(error.message, std::string("\u201ca.txt\u201d is open in another tab."));
    EXPECT_EQ(error.detail, std::string("Close that tab first, or choose another name."));
}

TEST(SaveMessages, SaveError) {
    OpenQuestion error = vermilion::save_error("a.txt", "Permission denied");
    EXPECT_EQ(error.message, std::string("Could not save \u201ca.txt\u201d"));
    EXPECT_EQ(error.detail, std::string("Permission denied"));
}

TEST(SaveMessages, StatusLines) {
    EXPECT_EQ(vermilion::saving_message("a.txt", 7),
              std::string("Saving \u201ca.txt\u201d\u2026 7%"));
    EXPECT_EQ(vermilion::saved_message("a.txt"), std::string("Saved \u201ca.txt\u201d."));
}

TEST(SaveMessages, BusyAndNextSave) {
    EXPECT_EQ(vermilion::busy_message("a.txt"),
              std::string("Wait until \u201ca.txt\u201d has finished loading or saving."));
    vermilion::TextFormat format;
    format.encoding = vermilion::TextEncoding::Utf16Be;
    format.bom = true;
    format.line_ending = vermilion::LineEnding::Cr;
    EXPECT_EQ(vermilion::next_save_message(format),
              std::string("The next save writes UTF-16 BE with CR line endings."));
}

} // namespace
