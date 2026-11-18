// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery copy of a document whose file was deleted or moved while it was open.

#include <gtest/gtest.h>

#include <optional>

#include "core/document_list.h"
#include "core/recovery_tracker.h"
#include "recovery_tracker_test_util.h"

namespace {

using tracker_test::RecoveryTrackerTest;
using vermilion::DocumentId;
using vermilion::RecoveryJob;
using vermilion::TextFormat;

// A deleted document has no unsaved edits, but its text exists nowhere else, so it gets a copy.
TEST_F(RecoveryTrackerTest, ADeletedCleanDocumentGetsACopy) {
    DocumentId id = list_.add_file("/tmp/gone.txt", "gone.txt", TextFormat{}, false);
    list_.set_deleted_on_disk(id, true);
    tracker_.note_edit(id, 10, 0);
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->document, id);
}

TEST_F(RecoveryTrackerTest, ACleanDocumentWithItsFileGetsNoCopy) {
    DocumentId id = list_.add_file("/tmp/here.txt", "here.txt", TextFormat{}, false);
    tracker_.note_edit(id, 10, 0);
    EXPECT_FALSE(begin(5000).has_value());
}

TEST_F(RecoveryTrackerTest, ADeletedReadOnlyDocumentStillGetsNoCopy) {
    DocumentId id = list_.add_file("/tmp/bin.dat", "bin.dat", TextFormat{}, false);
    list_.set_read_only(id, true);
    list_.set_deleted_on_disk(id, true);
    tracker_.note_edit(id, 10, 0);
    EXPECT_FALSE(begin(5000).has_value());
}

} // namespace
