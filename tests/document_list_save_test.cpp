// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for DocumentList after saving: Save As paths, the saved format and unsaved documents.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/text_format.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::LineEnding;
using vermilion::TextEncoding;
using vermilion::TextFormat;

TEST(DocumentListSave, SaveAsGivesUntitledAPath) {
    DocumentList list;
    DocumentId id = list.add_untitled();
    ASSERT_TRUE(list.set_path(id, "/tmp/notes.txt", "notes.txt"));
    EXPECT_EQ(list.find(id)->path, std::string("/tmp/notes.txt"));
    EXPECT_EQ(list.find(id)->title, std::string("notes.txt"));
    EXPECT_EQ(list.find(id)->untitled_number, 0);
    EXPECT_EQ(list.find_by_path("/tmp/notes.txt")->id, id);
}

TEST(DocumentListSave, SavedUntitledFreesItsNumber) {
    DocumentList list;
    DocumentId first = list.add_untitled();
    list.add_untitled();
    ASSERT_TRUE(list.set_path(first, "/tmp/a.txt", "a.txt"));
    DocumentId next = list.add_untitled();
    EXPECT_EQ(list.find(next)->title, std::string("Untitled 1"));
}

TEST(DocumentListSave, SaveAsRefusesAPathOpenElsewhere) {
    DocumentList list;
    list.add_file("/tmp/a.txt", "a.txt", TextFormat{}, false);
    DocumentId other = list.add_untitled();
    EXPECT_FALSE(list.set_path(other, "/tmp/a.txt", "a.txt"));
    EXPECT_TRUE(list.find(other)->path.empty());
    EXPECT_FALSE(list.set_path(other, "", "x"));
    EXPECT_FALSE(list.set_path(other + 100, "/tmp/b.txt", "b.txt"));
}

TEST(DocumentListSave, SaveAsToItsOwnPathIsAllowed) {
    DocumentList list;
    DocumentId id = list.add_file("/tmp/a.txt", "a.txt", TextFormat{}, false);
    EXPECT_TRUE(list.set_path(id, "/tmp/a.txt", "a.txt"));
}

TEST(DocumentListSave, SetFormatAndMixedFlag) {
    DocumentList list;
    DocumentId id = list.add_file("/tmp/m.txt", "m.txt", TextFormat{}, true);
    TextFormat format;
    format.encoding = TextEncoding::Utf16Le;
    format.bom = true;
    format.line_ending = LineEnding::CrLf;
    ASSERT_TRUE(list.set_format(id, format, false));
    EXPECT_EQ(list.find(id)->format.encoding, TextEncoding::Utf16Le);
    EXPECT_TRUE(list.find(id)->format.bom);
    EXPECT_EQ(list.find(id)->format.line_ending, LineEnding::CrLf);
    EXPECT_FALSE(list.find(id)->mixed_line_endings);
    ASSERT_TRUE(list.set_format(id, format, true));
    EXPECT_TRUE(list.find(id)->mixed_line_endings);
    EXPECT_FALSE(list.set_format(id + 100, format, false));
}

TEST(DocumentListSave, ModifiedIdsInTabOrder) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    DocumentId b = list.add_untitled();
    DocumentId c = list.add_untitled();
    EXPECT_TRUE(list.modified_ids().empty());
    list.set_modified(c, true);
    list.set_modified(a, true);
    list.move(c, 0);
    EXPECT_EQ(list.modified_ids(), (std::vector<DocumentId>{c, a}));
    list.set_modified(b, true);
    list.set_modified(c, false);
    EXPECT_EQ(list.modified_ids(), (std::vector<DocumentId>{a, b}));
}

} // namespace
