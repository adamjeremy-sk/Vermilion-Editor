// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the start-up question: a copy's file and name, the file's state and the texts.

#include <gtest/gtest.h>

#include <glib.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "core/open_messages.h"
#include "core/recovery_file.h"
#include "core/recovery_offer.h"
#include "core/recovery_question.h"

namespace {

using vermilion::FileState;
using vermilion::RecoveryHeader;
using vermilion::RecoveryItem;
using vermilion::RecoveryQuestion;
using Time = std::optional<std::int64_t>;

// The message of a valid copy without its number; the quotes are the ones of quoted().
std::string recover_text(const std::string& name) {
    return "Recover unsaved changes to " + vermilion::quoted(name) + "?";
}

constexpr std::int64_t kCopied = 1794452733554000; // 2026-11-12 03:05:33 UTC
constexpr std::int64_t kBased = 1794450001123456;  // 2026-11-12 02:20:01 UTC
constexpr std::int64_t kLater = 1794451000000000;  // 2026-11-12 02:36:40 UTC

RecoveryHeader untitled() {
    RecoveryHeader header;
    header.title = "Untitled 3";
    header.copied_us = kCopied;
    return header;
}

RecoveryHeader file(const std::string& uri = "file:///home/adam/notes/todo.txt") {
    RecoveryHeader header;
    header.uri = uri;
    header.copied_us = kCopied;
    header.file_mtime_us = kBased;
    return header;
}

RecoveryItem valid(const RecoveryHeader& header) {
    RecoveryItem item;
    item.id = "aaaaaaaa-0000-4000-8000-000000000001";
    item.header = header;
    return item;
}

RecoveryQuestion ask(const RecoveryHeader& header, Time now, std::size_t number = 1,
                     std::size_t total = 1) {
    return vermilion::recovery_question(valid(header), number, total, now, false);
}

TEST(RecoveryPath, ComesFromAFileUriAndNothingElse) {
    EXPECT_EQ(vermilion::recovery_path(file()),
              std::optional<std::string>("/home/adam/notes/todo.txt"));
    EXPECT_EQ(vermilion::recovery_path(file("file:///home/adam/my%20file.txt")),
              std::optional<std::string>("/home/adam/my file.txt"));
    EXPECT_FALSE(vermilion::recovery_path(untitled()).has_value());
    EXPECT_FALSE(vermilion::recovery_path(file("http://example.com/a.txt")).has_value());
}

TEST(RecoveryPath, NamesTheDocumentForTheUser) {
    EXPECT_EQ(vermilion::recovery_name(file()), "todo.txt");
    EXPECT_EQ(vermilion::recovery_name(file("file:///home/adam/my%20file.txt")), "my file.txt");
    EXPECT_EQ(vermilion::recovery_name(untitled()), "Untitled 3");
    EXPECT_EQ(vermilion::recovery_name(file("http://example.com/a.txt")),
              "http://example.com/a.txt");
    EXPECT_EQ(vermilion::recovery_name(RecoveryHeader{}), "Untitled");
}

TEST(RecoveryFileState, IsDecidedInTheDocumentedOrder) {
    EXPECT_EQ(vermilion::file_state(untitled(), std::nullopt), FileState::Untitled);
    EXPECT_EQ(vermilion::file_state(untitled(), Time(kLater)), FileState::Untitled);
    EXPECT_EQ(vermilion::file_state(file(), std::nullopt), FileState::Missing);
    RecoveryHeader no_time = file();
    no_time.file_mtime_us = std::nullopt;
    EXPECT_EQ(vermilion::file_state(no_time, std::nullopt), FileState::Missing);
    EXPECT_EQ(vermilion::file_state(no_time, Time(kLater)), FileState::Unknown);
    EXPECT_EQ(vermilion::file_state(file(), Time(kBased)), FileState::Unchanged);
    EXPECT_EQ(vermilion::file_state(file(), Time(kBased + 1)), FileState::Changed);
    EXPECT_EQ(vermilion::file_state(file(), Time(kBased - 1)), FileState::Changed);
}

TEST(RecoveryTime, IsWrittenInWholeSecondsAndUtcOnRequest) {
    EXPECT_EQ(vermilion::format_recovery_time(kCopied, false), "2026-11-12 03:05:33");
    EXPECT_EQ(vermilion::format_recovery_time(0, false), "1970-01-01 00:00:00");
    EXPECT_EQ(vermilion::format_recovery_time(999999, false), "1970-01-01 00:00:00");
    EXPECT_EQ(vermilion::format_recovery_time(1000000, false), "1970-01-01 00:00:01");
}

TEST(RecoveryTime, AnImpossibleTimeIsSaidSo) {
    EXPECT_EQ(vermilion::format_recovery_time(-1, false), "an unknown time");
    EXPECT_EQ(vermilion::format_recovery_time(std::numeric_limits<std::int64_t>::max(), false),
              "an unknown time");
    EXPECT_EQ(vermilion::format_recovery_time(-1, true), "an unknown time");
}

TEST(RecoveryTime, LocalTimeFollowsTheTimeZone) {
    const char* before = g_getenv("TZ");
    std::string saved = before != nullptr ? before : "";
    ASSERT_TRUE(g_setenv("TZ", "XYZ-3", TRUE)); // three hours ahead of UTC
    EXPECT_EQ(vermilion::format_recovery_time(kCopied, true), "2026-11-12 06:05:33");
    EXPECT_EQ(vermilion::format_recovery_time(kCopied, false), "2026-11-12 03:05:33");
    if (before != nullptr) {
        g_setenv("TZ", saved.c_str(), TRUE);
    } else {
        g_unsetenv("TZ");
    }
}

TEST(RecoveryQuestionText, AnUntitledCopy) {
    RecoveryQuestion question = ask(untitled(), std::nullopt);
    EXPECT_EQ(question.message, recover_text("Untitled 3"));
    EXPECT_EQ(question.detail,
              "The copy was made 2026-11-12 03:05:33.\nThe document was never saved to a file.");
}

TEST(RecoveryQuestionText, TheNumberOfTheCopyIsShownOnlyWhenThereAreSeveral) {
    EXPECT_EQ(ask(untitled(), std::nullopt, 1, 1).message, recover_text("Untitled 3"));
    EXPECT_EQ(ask(untitled(), std::nullopt, 1, 2).message,
              "Recover unsaved changes to \u201cUntitled 3\u201d? (1 of 2)");
    EXPECT_EQ(ask(untitled(), std::nullopt, 2, 3).message,
              recover_text("Untitled 3") + " (2 of 3)");
}

TEST(RecoveryQuestionText, AFileThatHasNotChanged) {
    RecoveryQuestion question = ask(file(), Time(kBased));
    EXPECT_EQ(question.message, recover_text("todo.txt"));
    EXPECT_EQ(question.detail, "The copy was made 2026-11-12 03:05:33.\n"
                               "File: /home/adam/notes/todo.txt\n"
                               "The file has not changed since the copy's text was based on it.");
}

TEST(RecoveryQuestionText, AFileThatHasChanged) {
    EXPECT_EQ(ask(file(), Time(kLater)).detail,
              "The copy was made 2026-11-12 03:05:33.\n"
              "File: /home/adam/notes/todo.txt\n"
              "The file was last changed 2026-11-12 02:36:40, after the copy's text was based on "
              "it. Saving will ask before it overwrites the file.");
}

TEST(RecoveryQuestionText, AFileThatIsGone) {
    EXPECT_EQ(ask(file(), std::nullopt).detail,
              "The copy was made 2026-11-12 03:05:33.\n"
              "File: /home/adam/notes/todo.txt\n"
              "The file no longer exists or cannot be read. Saving will write it again.");
}

TEST(RecoveryQuestionText, AFileWhoseBaseTimeWasNotRecorded) {
    RecoveryHeader header = file();
    header.file_mtime_us = std::nullopt;
    EXPECT_EQ(ask(header, Time(kLater)).detail,
              "The copy was made 2026-11-12 03:05:33.\n"
              "File: /home/adam/notes/todo.txt\n"
              "The copy does not record which version of the file its text was based on. "
              "The file was last changed 2026-11-12 02:36:40.");
}

TEST(RecoveryQuestionText, AUriThatIsNotALocalFileIsShownAsItIs) {
    EXPECT_EQ(ask(file("http://example.com/a.txt"), std::nullopt).detail,
              "The copy was made 2026-11-12 03:05:33.\n"
              "File: http://example.com/a.txt\n"
              "The file no longer exists or cannot be read. Saving will write it again.");
}

TEST(RecoveryQuestionText, ADamagedCopy) {
    RecoveryItem item;
    item.id = "ccc-bad";
    item.damaged = true;
    item.problem = "damaged recovery file: not a recovery file";
    RecoveryQuestion one = vermilion::recovery_question(item, 1, 1, Time(kLater), false);
    EXPECT_EQ(one.message, "A recovery copy is damaged.");
    EXPECT_EQ(one.detail,
              "The copy ccc-bad.vrec cannot be used: damaged recovery file: not a recovery file\n"
              "Decide Later keeps it; Discard deletes it.");
    RecoveryQuestion of_four = vermilion::recovery_question(item, 4, 4, std::nullopt, false);
    EXPECT_EQ(of_four.message, "A recovery copy is damaged. (4 of 4)");
    EXPECT_EQ(of_four.detail, one.detail);
}

} // namespace
