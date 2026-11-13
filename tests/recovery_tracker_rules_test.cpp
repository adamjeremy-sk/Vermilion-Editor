// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery tracker's rules: ids, which documents get copies and the size ceiling.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_store.h"
#include "core/recovery_tracker.h"
#include "recovery_tracker_test_util.h"

namespace {

using tracker_test::kCeiling;
using tracker_test::kCopied;
using tracker_test::nothing;
using tracker_test::RecoveryTrackerTest;
using vermilion::DocumentId;
using vermilion::LineEnding;
using vermilion::Millis;
using vermilion::RecoveryEffects;
using vermilion::RecoveryJob;
using vermilion::RecoveryTracker;
using vermilion::TextEncoding;
using vermilion::TextFormat;
using Ids = std::vector<std::string>;

TEST_F(RecoveryTrackerTest, UnknownDocumentsAreIgnored) {
    EXPECT_TRUE(nothing(tracker_.note_edit(99, 10, 0)));
    EXPECT_TRUE(nothing(tracker_.note_clean(99)));
    EXPECT_TRUE(nothing(tracker_.note_closed(99)));
    tracker_.set_file_mtime(99, 5);
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(tracker_.recovery_id_of(99).has_value());
    EXPECT_EQ(made_, 0);
}

TEST_F(RecoveryTrackerTest, TheIdIsMadeOnceForADocumentAndKept) {
    DocumentId a = edited_untitled(0);
    DocumentId b = edited_untitled(1000);
    EXPECT_EQ(made_, 2);
    EXPECT_EQ(tracker_.recovery_id_of(a), std::optional<std::string>("id-1"));
    EXPECT_EQ(tracker_.recovery_id_of(b), std::optional<std::string>("id-2"));
    tracker_.note_edit(a, 11, 500);
    EXPECT_EQ(begin(7000)->recovery_id, "id-1");
    tracker_.end_copy(7000, 0, true);
    tracker_.note_edit(a, 12, 8000);
    EXPECT_EQ(made_, 2);
    EXPECT_EQ(tracker_.recovery_id_of(a), std::optional<std::string>("id-1"));
}

TEST_F(RecoveryTrackerTest, NoIdIsMadeUntilACopyIsNeeded) {
    DocumentId file = list_.add_file("/home/adam/a.txt", "a.txt", TextFormat{}, false);
    tracker_.set_file_mtime(file, 1);
    DocumentId read_only = list_.add_untitled();
    list_.set_read_only(read_only, true);
    tracker_.note_edit(read_only, 10, 0);
    DocumentId large = list_.add_untitled();
    tracker_.note_edit(large, kCeiling + 1, 0);
    EXPECT_EQ(made_, 0);
    EXPECT_FALSE(tracker_.recovery_id_of(large).has_value());
    list_.set_modified(file, true);
    tracker_.note_edit(file, 10, 0);
    EXPECT_EQ(made_, 1);
}

TEST_F(RecoveryTrackerTest, TheDefaultIdsAreValidAndDifferent) {
    RecoveryTracker plain(list_);
    DocumentId a = list_.add_untitled();
    DocumentId b = list_.add_untitled();
    plain.note_edit(a, 10, 0);
    plain.note_edit(b, 10, 0);
    std::optional<std::string> first = plain.recovery_id_of(a);
    std::optional<std::string> second = plain.recovery_id_of(b);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(vermilion::valid_recovery_id(*first));
    EXPECT_TRUE(vermilion::valid_recovery_id(*second));
    EXPECT_NE(*first, *second);
}

TEST_F(RecoveryTrackerTest, ADocumentThatIsNotModifiedWhenItsCopyIsDueGetsNone) {
    DocumentId id = edited_untitled();
    list_.set_modified(id, false);
    EXPECT_FALSE(begin(5000).has_value());
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(tracker_.copy_in_flight());
}

TEST_F(RecoveryTrackerTest, AReadOnlyDocumentGetsNoCopy) {
    DocumentId id = list_.add_untitled();
    list_.set_modified(id, true);
    list_.set_read_only(id, true);
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 10, 0)));
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(begin(100000).has_value());
}

TEST_F(RecoveryTrackerTest, ADocumentThatTurnsReadOnlyBeforeItsCopyIsDueGetsNone) {
    DocumentId id = edited_untitled();
    list_.set_read_only(id, true);
    EXPECT_FALSE(begin(5000).has_value());
    EXPECT_FALSE(tracker_.wake_at().has_value());
}

TEST_F(RecoveryTrackerTest, ReadOnlyStopsTheCopiesAndDeletesTheOneThatExists) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    list_.set_read_only(id, true);
    RecoveryEffects effects = tracker_.note_edit(id, 10, 6000);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_FALSE(effects.announce_too_large);
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_TRUE(tracker_.copy_ids().empty());
}

TEST_F(RecoveryTrackerTest, TheCeilingIsInclusiveAndAnnouncedOncePerEpisode) {
    DocumentId id = list_.add_untitled();
    list_.set_modified(id, true);
    EXPECT_TRUE(nothing(tracker_.note_edit(id, kCeiling, 0)));
    EXPECT_TRUE(tracker_.wake_at().has_value());
    RecoveryEffects over = tracker_.note_edit(id, kCeiling + 1, 100);
    EXPECT_TRUE(over.announce_too_large);
    EXPECT_FALSE(over.delete_id.has_value());
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_TRUE(nothing(tracker_.note_edit(id, kCeiling + 5, 200)));
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 5, 300)));
    EXPECT_EQ(tracker_.wake_at(), std::optional<Millis>(5300));
    EXPECT_TRUE(tracker_.note_edit(id, kCeiling + 1, 400).announce_too_large);
}

TEST_F(RecoveryTrackerTest, ASavedDocumentAnnouncesAgainWhenItIsTooLargeAgain) {
    DocumentId id = list_.add_untitled();
    EXPECT_TRUE(tracker_.note_edit(id, kCeiling + 1, 0).announce_too_large);
    EXPECT_FALSE(tracker_.note_edit(id, kCeiling + 1, 10).announce_too_large);
    EXPECT_TRUE(nothing(tracker_.note_clean(id)));
    EXPECT_TRUE(tracker_.note_edit(id, kCeiling + 1, 20).announce_too_large);
}

TEST_F(RecoveryTrackerTest, GoingOverTheCeilingDeletesTheCopyAndStopsTheCopies) {
    DocumentId id = edited_untitled();
    write_copy(5000);
    EXPECT_EQ(tracker_.copy_ids(), (Ids{"id-1"}));
    RecoveryEffects effects = tracker_.note_edit(id, kCeiling + 1, 6000);
    EXPECT_EQ(effects.delete_id, std::optional<std::string>("id-1"));
    EXPECT_TRUE(effects.announce_too_large);
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(begin(100000).has_value());
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 10, 7000)));
    EXPECT_EQ(tracker_.recovery_id_of(id), std::optional<std::string>("id-1"));
    EXPECT_EQ(begin(12000)->recovery_id, "id-1");
}

} // namespace
