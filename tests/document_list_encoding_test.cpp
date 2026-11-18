// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the flag that says the user chose a document's encoding.

#include <gtest/gtest.h>

#include "core/document_list.h"
#include "core/text_format.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::TextFormat;

TEST(DocumentListEncoding, AFreshRecordHasNoChosenEncoding) {
    DocumentList list;
    DocumentId id = list.add_file("/tmp/notes.txt", "notes.txt", TextFormat{}, false);
    EXPECT_FALSE(list.find(id)->encoding_chosen);
}

TEST(DocumentListEncoding, TheFlagCanBeSetAndCleared) {
    DocumentList list;
    DocumentId id = list.add_file("/tmp/notes.txt", "notes.txt", TextFormat{}, false);
    EXPECT_TRUE(list.set_encoding_chosen(id, true));
    EXPECT_TRUE(list.find(id)->encoding_chosen);
    EXPECT_TRUE(list.set_encoding_chosen(id, false));
    EXPECT_FALSE(list.find(id)->encoding_chosen);
}

TEST(DocumentListEncoding, AnUnknownIdIsRefused) {
    DocumentList list;
    EXPECT_FALSE(list.set_encoding_chosen(42, true));
}

} // namespace
