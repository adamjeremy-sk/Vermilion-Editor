// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for adopting a recovered copy: the id is kept, the copy counts as existing and is deleted.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_tracker.h"
#include "core/text_format.h"
#include "recovery_tracker_test_util.h"

namespace {

using tracker_test::RecoveryTrackerTest;
using vermilion::DocumentId;
using vermilion::RecoveryEffects;
using vermilion::RecoveryJob;
using vermilion::TextFormat;

constexpr const char* kOldId = "0a1b2c3d-0000-4000-8000-000000000001";
constexpr const char* kOtherId = "0a1b2c3d-0000-4000-8000-000000000002";

TEST_F(RecoveryTrackerTest, AnAdoptedCopyKeepsItsIdAndCountsAsWritten) {
    DocumentId id = list_.add_file("/home/adam/a.txt", "a.txt", TextFormat{}, false);
    list_.set_modified(id, true);
    EXPECT_TRUE(tracker_.adopt_copy(id, kOldId, 1234));
    EXPECT_EQ(tracker_.recovery_id_of(id), std::optional<std::string>(kOldId));
    EXPECT_EQ(tracker_.copy_ids(), std::vector<std::string>({kOldId}));
    EXPECT_EQ(made_, 0);
}

TEST_F(RecoveryTrackerTest, TheNextCopyReplacesTheAdoptedFile) {
    DocumentId id = list_.add_file("/home/adam/a.txt", "a.txt", TextFormat{}, false);
    list_.set_modified(id, true);
    ASSERT_TRUE(tracker_.adopt_copy(id, kOldId, 1234));
    tracker_.note_edit(id, 10, 1000);
    std::optional<RecoveryJob> job = begin(6000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->recovery_id, std::string(kOldId));
    EXPECT_EQ(job->header.file_mtime_us, std::optional<std::int64_t>(1234));
    EXPECT_EQ(made_, 0);
}

TEST_F(RecoveryTrackerTest, CleaningOrClosingAnAdoptedDocumentDeletesItsCopy) {
    DocumentId a = list_.add_untitled();
    DocumentId b = list_.add_untitled();
    list_.set_modified(a, true);
    list_.set_modified(b, true);
    ASSERT_TRUE(tracker_.adopt_copy(a, kOldId, std::nullopt));
    ASSERT_TRUE(tracker_.adopt_copy(b, kOtherId, std::nullopt));
    RecoveryEffects cleaned = tracker_.note_clean(a);
    EXPECT_EQ(cleaned.delete_id, std::optional<std::string>(kOldId));
    RecoveryEffects closed = tracker_.note_closed(b);
    EXPECT_EQ(closed.delete_id, std::optional<std::string>(kOtherId));
    EXPECT_TRUE(tracker_.copy_ids().empty());
}

TEST_F(RecoveryTrackerTest, AdoptingIsRefusedWithoutChangingAnything) {
    DocumentId id = list_.add_untitled();
    DocumentId other = list_.add_untitled();
    EXPECT_FALSE(tracker_.adopt_copy(999, kOldId, std::nullopt));
    EXPECT_FALSE(tracker_.adopt_copy(id, "../evil", std::nullopt));
    EXPECT_FALSE(tracker_.adopt_copy(id, "", std::nullopt));
    EXPECT_FALSE(tracker_.recovery_id_of(id).has_value());
    ASSERT_TRUE(tracker_.adopt_copy(id, kOldId, std::nullopt));
    EXPECT_FALSE(tracker_.adopt_copy(other, kOldId, std::nullopt));
    EXPECT_FALSE(tracker_.recovery_id_of(other).has_value());
    EXPECT_FALSE(tracker_.adopt_copy(id, kOtherId, std::nullopt));
    EXPECT_EQ(tracker_.recovery_id_of(id), std::optional<std::string>(kOldId));
}

TEST_F(RecoveryTrackerTest, ADocumentThatAlreadyHasAnIdCannotAdopt) {
    DocumentId id = edited_untitled(0);
    EXPECT_FALSE(tracker_.adopt_copy(id, kOldId, std::nullopt));
    EXPECT_EQ(tracker_.recovery_id_of(id), std::optional<std::string>("id-1"));
}

} // namespace
