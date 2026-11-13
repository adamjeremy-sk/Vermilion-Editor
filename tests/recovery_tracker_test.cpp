// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery tracker's copy headers: the uri or title, the format and the file time.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_file.h"
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

TEST_F(RecoveryTrackerTest, NothingHappensBeforeAnEdit) {
    EXPECT_FALSE(tracker_.wake_at().has_value());
    EXPECT_FALSE(begin(1000000).has_value());
    EXPECT_FALSE(tracker_.copy_in_flight());
    EXPECT_TRUE(tracker_.copy_ids().empty());
    EXPECT_FALSE(tracker_.recovery_id_of(1).has_value());
}

TEST_F(RecoveryTrackerTest, AnEditedUntitledDocumentGetsACopyAfterTheIdleTime) {
    DocumentId id = list_.add_untitled();
    list_.set_modified(id, true);
    EXPECT_TRUE(nothing(tracker_.note_edit(id, 10, 1000)));
    EXPECT_EQ(tracker_.wake_at(), std::optional<Millis>(6000));
    EXPECT_FALSE(begin(5999).has_value());
    std::optional<RecoveryJob> job = begin(6000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->document, id);
    EXPECT_EQ(job->recovery_id, "id-1");
    EXPECT_EQ(job->header.title, std::optional<std::string>("Untitled 1"));
    EXPECT_FALSE(job->header.uri.has_value());
    EXPECT_FALSE(job->header.file_mtime_us.has_value());
    EXPECT_EQ(job->header.copied_us, kCopied);
    EXPECT_EQ(job->header.length, std::uint64_t(0));
    EXPECT_EQ(job->header.format.encoding, TextEncoding::Utf8);
    EXPECT_FALSE(job->header.format.bom);
    EXPECT_EQ(job->header.format.line_ending, LineEnding::Lf);
    EXPECT_FALSE(job->header.mixed_line_endings);
    EXPECT_TRUE(tracker_.copy_in_flight());
}

TEST_F(RecoveryTrackerTest, AFileDocumentGetsItsUriItsFormatAndItsFileTime) {
    TextFormat format;
    format.encoding = TextEncoding::Utf16Le;
    format.bom = true;
    format.line_ending = LineEnding::CrLf;
    DocumentId id = list_.add_file("/home/adam/My Notes/a b.txt", "a b.txt", format, true);
    list_.set_modified(id, true);
    tracker_.set_file_mtime(id, 555);
    tracker_.note_edit(id, 10, 0);
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->header.uri,
              std::optional<std::string>("file:///home/adam/My%20Notes/a%20b.txt"));
    EXPECT_FALSE(job->header.title.has_value());
    EXPECT_EQ(job->header.file_mtime_us, std::optional<std::int64_t>(555));
    EXPECT_EQ(job->header.format.encoding, TextEncoding::Utf16Le);
    EXPECT_TRUE(job->header.format.bom);
    EXPECT_EQ(job->header.format.line_ending, LineEnding::CrLf);
    EXPECT_TRUE(job->header.mixed_line_endings);
}

TEST_F(RecoveryTrackerTest, ThePathIsPercentEncoded) {
    edited_file("/home/adam/my notes/\xC3\xA4#?.txt");
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->header.uri,
              std::optional<std::string>("file:///home/adam/my%20notes/%C3%A4%23%3F.txt"));
}

TEST_F(RecoveryTrackerTest, TheHeaderIsMadeFromTheRecordWhenTheCopyBegins) {
    DocumentId id = edited_untitled();
    TextFormat format;
    format.encoding = TextEncoding::Latin1;
    format.line_ending = LineEnding::Cr;
    ASSERT_TRUE(list_.set_path(id, "/home/adam/saved.txt", "saved.txt"));
    ASSERT_TRUE(list_.set_format(id, format, true));
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->header.uri, std::optional<std::string>("file:///home/adam/saved.txt"));
    EXPECT_FALSE(job->header.title.has_value());
    EXPECT_EQ(job->header.format.encoding, TextEncoding::Latin1);
    EXPECT_EQ(job->header.format.line_ending, LineEnding::Cr);
    EXPECT_TRUE(job->header.mixed_line_endings);
}

TEST_F(RecoveryTrackerTest, TheFileTimeCanBeReplacedClearedAndIsNotWrittenForUntitled) {
    DocumentId id = edited_file("/home/adam/a.txt");
    tracker_.set_file_mtime(id, 100);
    tracker_.set_file_mtime(id, 200);
    EXPECT_EQ(begin(5000)->header.file_mtime_us, std::optional<std::int64_t>(200));
    tracker_.end_copy(5000, 0, true);
    tracker_.note_edit(id, 10, 6000);
    tracker_.set_file_mtime(id, std::nullopt);
    EXPECT_FALSE(begin(11000)->header.file_mtime_us.has_value());
    tracker_.end_copy(11000, 0, true);
    DocumentId untitled = edited_untitled(20000);
    tracker_.set_file_mtime(untitled, 300);
    std::optional<RecoveryJob> job = begin(25000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->document, untitled);
    EXPECT_FALSE(job->header.file_mtime_us.has_value());
}

TEST_F(RecoveryTrackerTest, TheFileTimeOfADocumentThatIsNotThereYetIsIgnored) {
    edited_untitled();
    tracker_.set_file_mtime(2, 77);
    DocumentId id = list_.add_file("/home/adam/b.txt", "b.txt", TextFormat{}, false);
    ASSERT_EQ(id, DocumentId(2));
    list_.set_modified(id, true);
    tracker_.note_edit(id, 10, 0);
    ASSERT_TRUE(begin(5000).has_value());
    tracker_.end_copy(5000, 0, true);
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->document, id);
    EXPECT_FALSE(job->header.file_mtime_us.has_value());
}

TEST_F(RecoveryTrackerTest, ATitleIsUsedWhenThePathCannotBeAUri) {
    DocumentId id = list_.add_file("relative.txt", "relative.txt", TextFormat{}, false);
    list_.set_modified(id, true);
    tracker_.note_edit(id, 10, 0);
    std::optional<RecoveryJob> job = begin(5000);
    ASSERT_TRUE(job.has_value());
    EXPECT_FALSE(job->header.uri.has_value());
    EXPECT_EQ(job->header.title, std::optional<std::string>("relative.txt"));
}

TEST_F(RecoveryTrackerTest, ATitleNeverBreaksTheHeaderLine) {
    DocumentId a = list_.add_file("rel-a", std::string("a\nb\rc\0d", 7), TextFormat{}, false);
    DocumentId b = list_.add_file("rel-b", "", TextFormat{}, false);
    for (DocumentId id : {a, b}) {
        list_.set_modified(id, true);
        tracker_.note_edit(id, 10, 0);
    }
    std::optional<RecoveryJob> first = begin(5000);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->header.title, std::optional<std::string>("a b c d"));
    tracker_.end_copy(5000, 0, true);
    std::optional<RecoveryJob> second = begin(5000);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->header.title, std::optional<std::string>("Untitled"));
}

TEST_F(RecoveryTrackerTest, EveryHeaderTheTrackerMakesCanBeWrittenAndReadBack) {
    TextFormat format;
    format.encoding = TextEncoding::Utf16Be;
    format.bom = true;
    DocumentId a = list_.add_untitled();
    DocumentId b = list_.add_file("/tmp/\xC3\xA9 \xD1\x84.txt", "f.txt", format, true);
    DocumentId c = list_.add_file("rel\n", "a\r\nb", TextFormat{}, false);
    tracker_.set_file_mtime(b, 42);
    for (DocumentId id : {a, b, c}) {
        list_.set_modified(id, true);
        tracker_.note_edit(id, 10, 0);
    }
    for (int i = 0; i < 3; ++i) {
        std::optional<RecoveryJob> job = begin(5000);
        ASSERT_TRUE(job.has_value());
        std::optional<std::string> head = vermilion::encode_header(job->header);
        ASSERT_TRUE(head.has_value());
        EXPECT_TRUE(vermilion::parse_header(*head, head->size()).is_ok());
        tracker_.end_copy(5000, 0, true);
    }
}

} // namespace
