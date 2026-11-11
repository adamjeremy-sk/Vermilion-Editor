// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for DocumentList's file documents: paths, formats, the open-once rule and read-only.

#include <gtest/gtest.h>

#include <string>

#include "core/document_list.h"
#include "core/text_format.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::DocumentRecord;
using vermilion::LineEnding;
using vermilion::TextEncoding;
using vermilion::TextFormat;

TextFormat crlf_windows1252() {
    TextFormat format;
    format.encoding = TextEncoding::Windows1252;
    format.line_ending = LineEnding::CrLf;
    return format;
}

TEST(DocumentListFiles, AddFileKeepsPathTitleAndFormat) {
    DocumentList list;
    DocumentId id = list.add_file("/home/a/notes.txt", "notes.txt", crlf_windows1252(), true);
    const DocumentRecord* record = list.find(id);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->path, std::string("/home/a/notes.txt"));
    EXPECT_EQ(record->title, std::string("notes.txt"));
    EXPECT_EQ(record->untitled_number, 0);
    EXPECT_EQ(record->format.encoding, TextEncoding::Windows1252);
    EXPECT_FALSE(record->format.bom);
    EXPECT_EQ(record->format.line_ending, LineEnding::CrLf);
    EXPECT_TRUE(record->mixed_line_endings);
    EXPECT_FALSE(record->modified);
    EXPECT_FALSE(record->read_only);
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), id);
}

TEST(DocumentListFiles, UntitledHasNoPathAndDefaultFormat) {
    DocumentList list;
    const DocumentRecord* record = list.find(list.add_untitled());
    ASSERT_NE(record, nullptr);
    EXPECT_TRUE(record->path.empty());
    EXPECT_EQ(record->format.encoding, TextEncoding::Utf8);
    EXPECT_FALSE(record->format.bom);
    EXPECT_EQ(record->format.line_ending, LineEnding::Lf);
    EXPECT_FALSE(record->mixed_line_endings);
    EXPECT_FALSE(record->read_only);
}

TEST(DocumentListFiles, FileIsAppendedAfterExistingTabs) {
    DocumentList list;
    DocumentId untitled = list.add_untitled();
    DocumentId file = list.add_file("/tmp/x.c", "x.c", TextFormat{}, false);
    ASSERT_EQ(list.records().size(), 2u);
    EXPECT_EQ(list.records()[0].id, untitled);
    EXPECT_EQ(list.records()[1].id, file);
}

TEST(DocumentListFiles, SamePathIsOpenedOnce) {
    DocumentList list;
    DocumentId first = list.add_file("/tmp/x.c", "x.c", TextFormat{}, false);
    list.add_untitled();
    DocumentId again = list.add_file("/tmp/x.c", "x.c", crlf_windows1252(), true);
    EXPECT_EQ(again, first);
    EXPECT_EQ(list.records().size(), 2u);
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), first);
    // The open document keeps the format it was opened with.
    EXPECT_EQ(list.find(first)->format.encoding, TextEncoding::Utf8);
    EXPECT_FALSE(list.find(first)->mixed_line_endings);
}

TEST(DocumentListFiles, FindByPath) {
    DocumentList list;
    DocumentId a = list.add_file("/tmp/a.txt", "a.txt", TextFormat{}, false);
    DocumentId b = list.add_file("/tmp/b.txt", "b.txt", TextFormat{}, false);
    list.add_untitled();
    ASSERT_NE(list.find_by_path("/tmp/b.txt"), nullptr);
    EXPECT_EQ(list.find_by_path("/tmp/b.txt")->id, b);
    EXPECT_EQ(list.find_by_path("/tmp/a.txt")->id, a);
    EXPECT_EQ(list.find_by_path("/tmp/A.txt"), nullptr);
    EXPECT_EQ(list.find_by_path(""), nullptr);
    ASSERT_TRUE(list.close(a));
    EXPECT_EQ(list.find_by_path("/tmp/a.txt"), nullptr);
}

TEST(DocumentListFiles, FilesDoNotTakeUntitledNumbers) {
    DocumentList list;
    list.add_file("/tmp/a.txt", "a.txt", TextFormat{}, false);
    DocumentId untitled = list.add_untitled();
    EXPECT_EQ(list.find(untitled)->title, std::string("Untitled 1"));
    EXPECT_EQ(list.find(untitled)->untitled_number, 1);
}

TEST(DocumentListFiles, ReadOnlyFlag) {
    DocumentList list;
    DocumentId id = list.add_file("/tmp/bin.dat", "bin.dat", TextFormat{}, false);
    ASSERT_TRUE(list.set_read_only(id, true));
    EXPECT_TRUE(list.find(id)->read_only);
    ASSERT_TRUE(list.set_read_only(id, false));
    EXPECT_FALSE(list.find(id)->read_only);
    EXPECT_FALSE(list.set_read_only(id + 100, true));
}

} // namespace
