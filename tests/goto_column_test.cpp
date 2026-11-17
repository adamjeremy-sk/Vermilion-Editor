// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for Go to Line's column rule: which character a typed column means.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/goto_column.h"

namespace {

using vermilion::goto_column_offset;

TEST(GotoColumn, ACharacterIsOneColumn) {
    EXPECT_EQ(goto_column_offset("hello", 4, 1), 0u);
    EXPECT_EQ(goto_column_offset("hello", 4, 3), 2u);
    EXPECT_EQ(goto_column_offset("hello", 4, 5), 4u);
}

TEST(GotoColumn, TheColumnJustAfterTheLastCharacterIsTheEnd) {
    EXPECT_EQ(goto_column_offset("hello", 4, 6), 5u);
}

TEST(GotoColumn, AColumnPastTheEndGoesToTheEnd) {
    EXPECT_EQ(goto_column_offset("hello", 4, 99), 5u);
    EXPECT_EQ(goto_column_offset("hello", 4, 999999999), 5u);
}

TEST(GotoColumn, AnEmptyLineIsAlwaysOffsetZero) {
    EXPECT_EQ(goto_column_offset("", 4, 1), 0u);
    EXPECT_EQ(goto_column_offset("", 4, 50), 0u);
}

TEST(GotoColumn, ColumnZeroIsTreatedAsOne) {
    EXPECT_EQ(goto_column_offset("hello", 4, 0), 0u);
}

TEST(GotoColumn, ATabCoversTheColumnsUpToTheNextTabStop) {
    // "a", then a tab over columns 2 to 4, then "b" in column 5.
    EXPECT_EQ(goto_column_offset("a\tb", 4, 1), 0u);
    EXPECT_EQ(goto_column_offset("a\tb", 4, 2), 1u);
    EXPECT_EQ(goto_column_offset("a\tb", 4, 3), 1u);
    EXPECT_EQ(goto_column_offset("a\tb", 4, 4), 1u);
    EXPECT_EQ(goto_column_offset("a\tb", 4, 5), 2u);
    EXPECT_EQ(goto_column_offset("a\tb", 4, 6), 3u);
}

TEST(GotoColumn, TwoTabsAtTabWidthEight) {
    EXPECT_EQ(goto_column_offset("\t\tx", 8, 8), 0u);
    EXPECT_EQ(goto_column_offset("\t\tx", 8, 9), 1u);
    EXPECT_EQ(goto_column_offset("\t\tx", 8, 16), 1u);
    EXPECT_EQ(goto_column_offset("\t\tx", 8, 17), 2u);
    EXPECT_EQ(goto_column_offset("\t\tx", 8, 18), 3u);
}

TEST(GotoColumn, ATabAfterTwoCharactersAtTabWidthTwoCoversTwoColumns) {
    // "ab" in columns 1 and 2, the tab over columns 3 and 4, "c" in column 5.
    EXPECT_EQ(goto_column_offset("ab\tc", 2, 3), 2u);
    EXPECT_EQ(goto_column_offset("ab\tc", 2, 4), 2u);
    EXPECT_EQ(goto_column_offset("ab\tc", 2, 5), 3u);
}

TEST(GotoColumn, ATabWidthOfZeroIsTreatedAsOne) {
    EXPECT_EQ(goto_column_offset("\ta", 0, 1), 0u);
    EXPECT_EQ(goto_column_offset("\ta", 0, 2), 1u);
}

TEST(GotoColumn, ACharacterIsACodePointNotAByte) {
    // U+00E9 is two bytes, U+65E5 U+672C U+8A9E are three bytes each; each is one column.
    EXPECT_EQ(goto_column_offset("\xC3\xA9\tx", 4, 2), 1u);
    EXPECT_EQ(goto_column_offset("\xC3\xA9\tx", 4, 5), 2u);
    EXPECT_EQ(goto_column_offset("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", 4, 2), 1u);
    EXPECT_EQ(goto_column_offset("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", 4, 4), 3u);
}

} // namespace
