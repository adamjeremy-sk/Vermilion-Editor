// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for PageHistory: opening pages, going back, the bound on the back list.

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>

#include "core/page_history.h"

namespace {

using vermilion::PageHistory;

TEST(PageHistory, StartsEmpty) {
    PageHistory history;
    EXPECT_EQ(history.current(), std::string());
    EXPECT_FALSE(history.can_go_back());
    EXPECT_FALSE(history.back().has_value());
}

TEST(PageHistory, FirstOpenHasNothingToGoBackTo) {
    PageHistory history;
    EXPECT_TRUE(history.open("index.md"));
    EXPECT_EQ(history.current(), std::string("index.md"));
    EXPECT_FALSE(history.can_go_back());
}

TEST(PageHistory, OpenThenBack) {
    PageHistory history;
    history.open("index.md");
    EXPECT_TRUE(history.open("a.md"));
    EXPECT_TRUE(history.open("b.md"));
    EXPECT_EQ(history.current(), std::string("b.md"));
    EXPECT_TRUE(history.can_go_back());
    EXPECT_EQ(history.back(), std::optional<std::string>("a.md"));
    EXPECT_EQ(history.current(), std::string("a.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
    EXPECT_FALSE(history.can_go_back());
    EXPECT_FALSE(history.back().has_value());
    EXPECT_EQ(history.current(), std::string("index.md"));
}

TEST(PageHistory, OpeningTheCurrentPageOrNothingChangesNothing) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    EXPECT_FALSE(history.open("a.md"));
    EXPECT_FALSE(history.open(""));
    EXPECT_EQ(history.current(), std::string("a.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
    EXPECT_FALSE(history.can_go_back());
}

TEST(PageHistory, ThePageLeftByBackIsNotKept) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.back();
    history.open("b.md");
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
    EXPECT_FALSE(history.can_go_back());
}

TEST(PageHistory, EarlierPagesMayComeAgain) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.open("index.md");
    EXPECT_EQ(history.back(), std::optional<std::string>("a.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
}

TEST(PageHistory, TheBackListIsBounded) {
    PageHistory history;
    for (int i = 0; i < 150; ++i) {
        EXPECT_TRUE(history.open("p" + std::to_string(i) + ".md"));
    }
    EXPECT_EQ(history.current(), std::string("p149.md"));
    std::size_t steps = 0;
    std::string last;
    while (steps < 2 * PageHistory::kMaxBack) {
        std::optional<std::string> page = history.back();
        if (!page) {
            break;
        }
        last = *page;
        ++steps;
    }
    EXPECT_EQ(steps, PageHistory::kMaxBack);
    EXPECT_EQ(last, std::string("p49.md"));
    EXPECT_EQ(history.current(), std::string("p49.md"));
}

} // namespace
