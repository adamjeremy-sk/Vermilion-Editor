// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the core version string.

#include <gtest/gtest.h>

#include <string>

#include "core/version.h"

TEST(CoreVersion, ReturnsExpectedVersion) {
    EXPECT_EQ(std::string(vermilion::version()), "0.0.1");
}
