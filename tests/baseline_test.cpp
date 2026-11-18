// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the save check against a document's baseline: when it applies and what it finds.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/baseline.h"
#include "core/file_stamp.h"

namespace {

using vermilion::Baseline;
using vermilion::DiskState;
using vermilion::FileStamp;
using Base = std::optional<Baseline>;

constexpr std::int64_t kBased = 1794450001123456;
constexpr std::uint64_t kBasedSize = 4096;
const char* const kPath = "/home/adam/notes/todo.txt";

Base base() {
    FileStamp stamp;
    stamp.mtime_us = kBased;
    stamp.size = kBasedSize;
    stamp.hash = 0xABCDEF;
    return Baseline{kPath, stamp};
}

Base recovered_base() {
    FileStamp stamp;
    stamp.mtime_us = kBased;
    stamp.time_only = true;
    return Baseline{kPath, stamp};
}

DiskState disk(std::int64_t mtime_us, std::uint64_t size) {
    DiskState state;
    state.exists = true;
    state.mtime_us = mtime_us;
    state.size = size;
    return state;
}

TEST(BaselineCheck, AppliesToASaveToTheSameFile) {
    EXPECT_TRUE(vermilion::needs_baseline_check(base(), kPath, false));
    EXPECT_TRUE(vermilion::needs_baseline_check(recovered_base(), kPath, false));
}

TEST(BaselineCheck, NeedsABaseline) {
    EXPECT_FALSE(vermilion::needs_baseline_check(std::nullopt, kPath, false));
}

TEST(BaselineCheck, SaveAnywaySkipsIt) {
    EXPECT_FALSE(vermilion::needs_baseline_check(base(), kPath, true));
}

TEST(BaselineCheck, SaveAsToAnotherFileSkipsIt) {
    EXPECT_FALSE(vermilion::needs_baseline_check(base(), "/home/adam/notes/other.txt", false));
    EXPECT_FALSE(vermilion::needs_baseline_check(base(), "/home/adam/notes/done.txt", false));
    EXPECT_FALSE(vermilion::needs_baseline_check(base(), "/home/adam/notes/todo.txt.bak", false));
    EXPECT_FALSE(vermilion::needs_baseline_check(base(), "", false));
}

TEST(BaselineCheck, ThePathIsComparedAsTextNotAsPrefix) {
    Base other = Baseline{"/home/adam/notes/todo.txt.bak", base()->stamp};
    EXPECT_FALSE(vermilion::needs_baseline_check(other, kPath, false));
    EXPECT_TRUE(vermilion::needs_baseline_check(other, "/home/adam/notes/todo.txt.bak", false));
}

TEST(BaselineChange, AnEqualTimeAndSizeIsNoChange) {
    EXPECT_FALSE(vermilion::changed_since_baseline(*base(), disk(kBased, kBasedSize)));
}

TEST(BaselineChange, AnyOtherTimeIsAChange) {
    EXPECT_TRUE(vermilion::changed_since_baseline(*base(), disk(kBased + 1, kBasedSize)));
    EXPECT_TRUE(vermilion::changed_since_baseline(*base(), disk(kBased - 1, kBasedSize)));
    EXPECT_TRUE(vermilion::changed_since_baseline(*base(), disk(0, kBasedSize)));
}

TEST(BaselineChange, AnotherSizeIsAChange) {
    EXPECT_TRUE(vermilion::changed_since_baseline(*base(), disk(kBased, kBasedSize + 1)));
    EXPECT_TRUE(vermilion::changed_since_baseline(*base(), disk(kBased, 0)));
}

TEST(BaselineChange, AFileThatIsGoneIsNoChange) {
    EXPECT_FALSE(vermilion::changed_since_baseline(*base(), DiskState{}));
    EXPECT_FALSE(vermilion::changed_since_baseline(*recovered_base(), DiskState{}));
}

TEST(BaselineChange, ATimeOnlyStampComparesTheTimeAlone) {
    EXPECT_FALSE(vermilion::changed_since_baseline(*recovered_base(), disk(kBased, 1)));
    EXPECT_FALSE(vermilion::changed_since_baseline(*recovered_base(), disk(kBased, 999999)));
    EXPECT_TRUE(vermilion::changed_since_baseline(*recovered_base(), disk(kBased + 1, 1)));
    EXPECT_TRUE(vermilion::changed_since_baseline(*recovered_base(), disk(kBased - 1, 0)));
}

} // namespace
