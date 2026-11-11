// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for open_messages: the question before opening, progress lines and the result message.

#include <gtest/gtest.h>

#include <string>

#include "core/document_list.h"
#include "core/open_messages.h"
#include "core/text_format.h"
#include "core/text_scan.h"

namespace {

using vermilion::DocumentRecord;
using vermilion::LineEnding;
using vermilion::OpenQuestion;
using vermilion::OpenWarning;
using vermilion::TextEncoding;

DocumentRecord record_named(const std::string& title) {
    DocumentRecord record;
    record.title = title;
    record.path = "/tmp/" + title;
    return record;
}

TEST(OpenMessages, Quoted) {
    EXPECT_EQ(vermilion::quoted("a.txt"), std::string("\u201ca.txt\u201d"));
}

TEST(OpenMessages, NoQuestionWithoutWarning) {
    OpenQuestion question = vermilion::open_question("a.txt", OpenWarning::None, 10);
    EXPECT_TRUE(question.message.empty());
    EXPECT_TRUE(question.detail.empty());
}

TEST(OpenMessages, BinaryQuestion) {
    OpenQuestion question = vermilion::open_question("a.bin", OpenWarning::Binary, 0);
    EXPECT_EQ(question.message, std::string("\u201ca.bin\u201d looks like a binary file."));
    EXPECT_EQ(question.detail, std::string("It contains NUL characters. If you open it anyway, "
                                           "it opens read-only."));
}

TEST(OpenMessages, LongLineQuestion) {
    OpenQuestion question = vermilion::open_question("one.txt", OpenWarning::LongLine, 2000000);
    EXPECT_EQ(question.message,
              std::string("\u201cone.txt\u201d has a line of 2000000 characters."));
    EXPECT_EQ(question.detail,
              std::string("Lines this long can make the editor slow or stop responding."));
}

TEST(OpenMessages, ProgressLines) {
    EXPECT_EQ(vermilion::checking_message("a.txt", 42),
              std::string("Checking \u201ca.txt\u201d\u2026 42%"));
    EXPECT_EQ(vermilion::loading_message("a.txt", 100),
              std::string("Loading \u201ca.txt\u201d\u2026 100%"));
}

TEST(OpenMessages, LoadedPlainUtf8) {
    EXPECT_EQ(vermilion::loaded_message(record_named("a.txt")),
              std::string("Opened \u201ca.txt\u201d."));
}

TEST(OpenMessages, LoadedWithEncodingAndBom) {
    DocumentRecord record = record_named("a.txt");
    record.format.encoding = TextEncoding::Windows1252;
    EXPECT_EQ(vermilion::loaded_message(record),
              std::string("Opened \u201ca.txt\u201d as Windows-1252."));
    record.format.encoding = TextEncoding::Utf8;
    record.format.bom = true;
    EXPECT_EQ(vermilion::loaded_message(record),
              std::string("Opened \u201ca.txt\u201d as UTF-8 with BOM."));
}

TEST(OpenMessages, LoadedWithMixedLineEndings) {
    DocumentRecord record = record_named("a.txt");
    record.format.encoding = TextEncoding::Utf16Le;
    record.format.bom = true;
    record.format.line_ending = LineEnding::CrLf;
    record.mixed_line_endings = true;
    EXPECT_EQ(vermilion::loaded_message(record),
              std::string("Opened \u201ca.txt\u201d as UTF-16 LE. It has mixed line endings; "
                          "saving makes them all CRLF."));
}

TEST(OpenMessages, LoadedReadOnlyWinsOverOtherNotes) {
    DocumentRecord record = record_named("a.bin");
    record.read_only = true;
    record.format.encoding = TextEncoding::Latin1;
    record.mixed_line_endings = true;
    EXPECT_EQ(vermilion::loaded_message(record),
              std::string("Opened \u201ca.bin\u201d read-only: it contains bytes that cannot be "
                          "shown as text."));
}

} // namespace
