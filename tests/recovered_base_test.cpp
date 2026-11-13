// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the first-save check of a recovered document: when it applies and what it finds.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/recovered_base.h"

namespace {

using vermilion::RecoveredBase;
using Base = std::optional<RecoveredBase>;
using Time = std::optional<std::int64_t>;

constexpr std::int64_t kBased = 1794450001123456;

Base base() {
    return RecoveredBase{"/home/adam/notes/todo.txt", kBased};
}

TEST(RecoveredBaseCheck, AppliesToTheFirstSaveToTheSameFile) {
    EXPECT_TRUE(vermilion::needs_base_check(base(), "/home/adam/notes/todo.txt", false));
}

TEST(RecoveredBaseCheck, NeedsABase) {
    EXPECT_FALSE(vermilion::needs_base_check(std::nullopt, "/home/adam/notes/todo.txt", false));
}

TEST(RecoveredBaseCheck, SaveAnywaySkipsIt) {
    EXPECT_FALSE(vermilion::needs_base_check(base(), "/home/adam/notes/todo.txt", true));
}

TEST(RecoveredBaseCheck, SaveAsToAnotherFileSkipsIt) {
    EXPECT_FALSE(vermilion::needs_base_check(base(), "/home/adam/notes/other.txt", false));
    EXPECT_FALSE(vermilion::needs_base_check(base(), "/home/adam/notes/done.txt", false));
    EXPECT_FALSE(vermilion::needs_base_check(base(), "/home/adam/notes/todo.txt.bak", false));
    EXPECT_FALSE(vermilion::needs_base_check(base(), "", false));
}

TEST(RecoveredBaseCheck, ThePathIsComparedAsTextNotAsPrefix) {
    Base other = RecoveredBase{"/home/adam/notes/todo.txt.bak", kBased};
    EXPECT_FALSE(vermilion::needs_base_check(other, "/home/adam/notes/todo.txt", false));
    EXPECT_TRUE(vermilion::needs_base_check(other, "/home/adam/notes/todo.txt.bak", false));
}

TEST(RecoveredBaseChange, AnEqualTimeIsNoChange) {
    EXPECT_FALSE(vermilion::file_changed_since_base(*base(), Time(kBased)));
}

TEST(RecoveredBaseChange, AnyOtherTimeIsAChange) {
    EXPECT_TRUE(vermilion::file_changed_since_base(*base(), Time(kBased + 1)));
    EXPECT_TRUE(vermilion::file_changed_since_base(*base(), Time(kBased - 1)));
    EXPECT_TRUE(vermilion::file_changed_since_base(*base(), Time(0)));
}

TEST(RecoveredBaseChange, AFileThatCannotBeReadIsNoChange) {
    EXPECT_FALSE(vermilion::file_changed_since_base(*base(), std::nullopt));
}

} // namespace
