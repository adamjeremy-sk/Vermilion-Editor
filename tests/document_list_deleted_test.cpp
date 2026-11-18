// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the deleted-on-disk flag of a document record and needs_save().

#include <gtest/gtest.h>

#include <vector>

#include "core/document_list.h"
#include "core/text_format.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::DocumentRecord;
using vermilion::TextFormat;

DocumentId add_file(DocumentList& list) {
    return list.add_file("/tmp/notes.txt", "notes.txt", TextFormat{}, false);
}

TEST(DocumentListDeleted, AFreshRecordIsNotDeleted) {
    DocumentList list;
    DocumentId id = add_file(list);
    EXPECT_FALSE(list.find(id)->deleted_on_disk);
    EXPECT_FALSE(list.find(id)->needs_save());
}

TEST(DocumentListDeleted, TheFlagCanBeSetAndCleared) {
    DocumentList list;
    DocumentId id = add_file(list);
    EXPECT_TRUE(list.set_deleted_on_disk(id, true));
    EXPECT_TRUE(list.find(id)->deleted_on_disk);
    EXPECT_TRUE(list.set_deleted_on_disk(id, false));
    EXPECT_FALSE(list.find(id)->deleted_on_disk);
}

TEST(DocumentListDeleted, AnUnknownIdIsRefused) {
    DocumentList list;
    EXPECT_FALSE(list.set_deleted_on_disk(42, true));
}

TEST(DocumentListDeleted, NeedsSaveIsModifiedOrDeleted) {
    DocumentList list;
    DocumentId id = add_file(list);
    list.set_modified(id, true);
    EXPECT_TRUE(list.find(id)->needs_save());
    list.set_modified(id, false);
    EXPECT_FALSE(list.find(id)->needs_save());
    list.set_deleted_on_disk(id, true);
    EXPECT_TRUE(list.find(id)->needs_save());
    list.set_modified(id, true);
    EXPECT_TRUE(list.find(id)->needs_save());
}

TEST(DocumentListDeleted, TheFlagLeavesTheModifiedFlagAloneButCountsAsNeedingASave) {
    DocumentList list;
    DocumentId id = add_file(list);
    list.set_deleted_on_disk(id, true);
    EXPECT_FALSE(list.find(id)->modified);
    EXPECT_EQ(list.modified_count(), 1u);
    EXPECT_EQ(list.modified_ids(), std::vector<DocumentId>{id});
    list.set_deleted_on_disk(id, false);
    EXPECT_EQ(list.modified_count(), 0u);
    EXPECT_TRUE(list.modified_ids().empty());
}

TEST(DocumentListDeleted, TheCountsSkipACleanDocumentAndCountEachOtherOneOnce) {
    DocumentList list;
    DocumentId edited = add_file(list);
    DocumentId clean = list.add_file("/tmp/b.txt", "b.txt", TextFormat{}, false);
    DocumentId gone = list.add_file("/tmp/c.txt", "c.txt", TextFormat{}, false);
    DocumentId both = list.add_file("/tmp/d.txt", "d.txt", TextFormat{}, false);
    list.set_modified(edited, true);
    list.set_deleted_on_disk(gone, true);
    list.set_modified(both, true);
    list.set_deleted_on_disk(both, true);
    EXPECT_EQ(list.modified_count(), 3u);
    EXPECT_EQ(list.modified_ids(), (std::vector<DocumentId>{edited, gone, both}));
    EXPECT_FALSE(list.find(clean)->needs_save());
}

TEST(DocumentListDeleted, TheFlagBelongsToOneDocument) {
    DocumentList list;
    DocumentId first = add_file(list);
    DocumentId second = list.add_file("/tmp/other.txt", "other.txt", TextFormat{}, false);
    list.set_deleted_on_disk(first, true);
    EXPECT_TRUE(list.find(first)->needs_save());
    EXPECT_FALSE(list.find(second)->needs_save());
}

TEST(DocumentListDeleted, AnUntitledDocumentStartsNotDeleted) {
    DocumentList list;
    DocumentId id = list.add_untitled();
    EXPECT_FALSE(list.find(id)->needs_save());
}

} // namespace
