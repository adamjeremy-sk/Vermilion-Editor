// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for counts written with commas between thousands.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/count_text.h"

namespace {

using vermilion::with_commas;

TEST(CountText, SmallNumbersHaveNoComma) {
    EXPECT_EQ(with_commas(0), std::string("0"));
    EXPECT_EQ(with_commas(7), std::string("7"));
    EXPECT_EQ(with_commas(42), std::string("42"));
    EXPECT_EQ(with_commas(999), std::string("999"));
}

TEST(CountText, ACommaSeparatesEachGroupOfThree) {
    EXPECT_EQ(with_commas(1000), std::string("1,000"));
    EXPECT_EQ(with_commas(12408), std::string("12,408"));
    EXPECT_EQ(with_commas(100000), std::string("100,000"));
    EXPECT_EQ(with_commas(1234567), std::string("1,234,567"));
    EXPECT_EQ(with_commas(1000000), std::string("1,000,000"));
}

TEST(CountText, TheLargestNumberWorks) {
    EXPECT_EQ(with_commas(UINT64_MAX), std::string("18,446,744,073,709,551,615"));
}

} // namespace
