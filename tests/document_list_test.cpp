// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for DocumentList: untitled numbering, the active document on close, flags and reordering.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/document_list.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;

std::vector<std::string> titles(const DocumentList& list) {
    std::vector<std::string> out;
    for (const vermilion::DocumentRecord& record : list.records()) {
        out.push_back(record.title);
    }
    return out;
}

TEST(DocumentList, FirstUntitledIsActiveAndUnmodified) {
    DocumentList list;
    DocumentId id = list.add_untitled();
    ASSERT_EQ(list.records().size(), 1u);
    EXPECT_EQ(list.records()[0].title, std::string("Untitled 1"));
    EXPECT_FALSE(list.records()[0].modified);
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), id);
    EXPECT_FALSE(list.empty());
}

TEST(DocumentList, UntitledTakesLowestFreeNumber) {
    DocumentList list;
    DocumentId one = list.add_untitled();
    DocumentId two = list.add_untitled();
    list.add_untitled();
    ASSERT_TRUE(list.close(one));
    ASSERT_TRUE(list.close(two));
    list.add_untitled();
    list.add_untitled();
    list.add_untitled();
    EXPECT_EQ(titles(list),
              (std::vector<std::string>{"Untitled 3", "Untitled 1", "Untitled 2", "Untitled 4"}));
}

TEST(DocumentList, ClosingActiveSelectsLeftNeighbour) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    DocumentId b = list.add_untitled();
    list.add_untitled();
    ASSERT_TRUE(list.set_active(b));
    ASSERT_TRUE(list.close(b));
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), a);
}

TEST(DocumentList, ClosingFirstPositionSelectsRightNeighbour) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    DocumentId b = list.add_untitled();
    list.add_untitled();
    ASSERT_TRUE(list.set_active(a));
    ASSERT_TRUE(list.close(a));
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), b);
}

TEST(DocumentList, ClosingInactiveKeepsActive) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    list.add_untitled();
    DocumentId c = list.add_untitled();
    ASSERT_TRUE(list.close(a));
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), c);
}

TEST(DocumentList, ClosingLastDocumentLeavesNoneActive) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    ASSERT_TRUE(list.close(a));
    EXPECT_TRUE(list.empty());
    EXPECT_FALSE(list.active().has_value());
}

TEST(DocumentList, UnknownIdIsRejected) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    DocumentId unknown = a + 100;
    EXPECT_FALSE(list.close(unknown));
    EXPECT_FALSE(list.set_active(unknown));
    EXPECT_FALSE(list.set_modified(unknown, true));
    EXPECT_FALSE(list.move(unknown, 0));
    EXPECT_EQ(list.find(unknown), nullptr);
    EXPECT_FALSE(list.index_of(unknown).has_value());
    EXPECT_EQ(list.records().size(), 1u);
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), a);
}

TEST(DocumentList, ModifiedFlagAndCount) {
    DocumentList list;
    list.add_untitled();
    DocumentId b = list.add_untitled();
    ASSERT_TRUE(list.set_modified(b, true));
    ASSERT_NE(list.find(b), nullptr);
    EXPECT_TRUE(list.find(b)->modified);
    EXPECT_EQ(list.modified_count(), 1u);
    ASSERT_TRUE(list.set_modified(b, false));
    EXPECT_EQ(list.modified_count(), 0u);
}

TEST(DocumentList, MoveReordersAndKeepsActive) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    list.add_untitled();
    DocumentId c = list.add_untitled();
    ASSERT_TRUE(list.move(a, 2));
    EXPECT_EQ(titles(list), (std::vector<std::string>{"Untitled 2", "Untitled 3", "Untitled 1"}));
    ASSERT_TRUE(list.move(a, 0));
    EXPECT_EQ(titles(list), (std::vector<std::string>{"Untitled 1", "Untitled 2", "Untitled 3"}));
    ASSERT_TRUE(list.move(a, 99));
    EXPECT_EQ(titles(list), (std::vector<std::string>{"Untitled 2", "Untitled 3", "Untitled 1"}));
    ASSERT_TRUE(list.index_of(a).has_value());
    EXPECT_EQ(*list.index_of(a), 2u);
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), c);
}

TEST(DocumentList, SetActiveSwitches) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    list.add_untitled();
    ASSERT_TRUE(list.set_active(a));
    ASSERT_TRUE(list.active().has_value());
    EXPECT_EQ(*list.active(), a);
}

TEST(DocumentList, IdsAreNeverReused) {
    DocumentList list;
    DocumentId a = list.add_untitled();
    ASSERT_TRUE(list.close(a));
    DocumentId b = list.add_untitled();
    EXPECT_NE(a, b);
    EXPECT_EQ(list.records()[0].title, std::string("Untitled 1"));
}

} // namespace
