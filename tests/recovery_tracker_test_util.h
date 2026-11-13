// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The fixture shared by the recovery tracker tests: a document list, a tracker and small helpers.

#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/document_list.h"
#include "core/recovery_schedule.h"
#include "core/recovery_tracker.h"

namespace tracker_test {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::Millis;
using vermilion::RecoveryEffects;
using vermilion::RecoveryJob;
using vermilion::RecoveryTracker;
using vermilion::TextFormat;

inline constexpr std::int64_t kCopied = 1794452733554000;
inline constexpr std::uint64_t kCeiling = vermilion::kRecoveryCeilingChars;

inline bool nothing(const RecoveryEffects& effects) {
    return !effects.delete_id && !effects.announce_too_large && !effects.announce_failure;
}

class RecoveryTrackerTest : public ::testing::Test {
protected:
    // An untitled document with unsaved changes, edited at `now`.
    DocumentId edited_untitled(Millis now = 0) {
        DocumentId id = list_.add_untitled();
        list_.set_modified(id, true);
        tracker_.note_edit(id, 10, now);
        return id;
    }

    // A file document with unsaved changes, edited at `now`.
    DocumentId edited_file(const std::string& path, Millis now = 0) {
        DocumentId id = list_.add_file(path, "file.txt", TextFormat{}, false);
        list_.set_modified(id, true);
        tracker_.note_edit(id, 10, now);
        return id;
    }

    std::optional<RecoveryJob> begin(Millis now) { return tracker_.begin_next(now, kCopied); }

    // A whole copy that works, with no hold of the main loop; the effects of its end.
    RecoveryEffects write_copy(Millis now) {
        EXPECT_TRUE(begin(now).has_value());
        return tracker_.end_copy(now, 0, true);
    }

    DocumentList list_;
    int made_ = 0;
    RecoveryTracker tracker_{list_, [this] { return "id-" + std::to_string(++made_); }};
};

} // namespace tracker_test
