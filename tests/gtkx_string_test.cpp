// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the gtkx string conversions.

#include <gtest/gtest.h>

#include <glib.h>

#include <string>

#include "ui/gtkx/string.h"

TEST(GtkxString, FromUtf8NullIsEmpty) {
    EXPECT_EQ(vermilion::gtkx::from_utf8(nullptr), std::string());
}

TEST(GtkxString, FromUtf8CopiesBytes) {
    const char* text = "h\xc3\xa9llo";
    EXPECT_EQ(vermilion::gtkx::from_utf8(text), std::string(text));
}

TEST(GtkxString, TakeStringFreesAndReturnsContent) {
    char* owned = g_strdup("h\xc3\xa9llo");
    EXPECT_EQ(vermilion::gtkx::take_string(owned), std::string("h\xc3\xa9llo"));
}

TEST(GtkxString, TakeStringNullIsEmpty) {
    EXPECT_EQ(vermilion::gtkx::take_string(nullptr), std::string());
}
