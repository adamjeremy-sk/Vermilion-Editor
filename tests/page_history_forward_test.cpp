// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the forward list of PageHistory: going forward, what clears it, its bound.

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>

#include "core/page_history.h"

namespace {

using vermilion::PageHistory;

TEST(PageHistoryForward, ThereIsNothingToGoForwardToAtFirst) {
    PageHistory history;
    EXPECT_FALSE(history.can_go_forward());
    EXPECT_FALSE(history.forward().has_value());
    history.open("index.md");
    history.open("a.md");
    EXPECT_FALSE(history.can_go_forward());
    EXPECT_FALSE(history.forward().has_value());
    EXPECT_EQ(history.current(), std::string("a.md"));
}

TEST(PageHistoryForward, BackThenForwardReturnsToThePageLeft) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.open("b.md");
    EXPECT_EQ(history.back(), std::optional<std::string>("a.md"));
    EXPECT_TRUE(history.can_go_forward());
    EXPECT_TRUE(history.can_go_back());
    EXPECT_EQ(history.forward(), std::optional<std::string>("b.md"));
    EXPECT_EQ(history.current(), std::string("b.md"));
    EXPECT_FALSE(history.can_go_forward());
    EXPECT_TRUE(history.can_go_back());
    EXPECT_FALSE(history.forward().has_value());
    EXPECT_EQ(history.current(), std::string("b.md"));
}

TEST(PageHistoryForward, ForwardWalksBackOverSeveralPagesInTheOrderTheyWereLeft) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.open("b.md");
    history.open("c.md");
    EXPECT_EQ(history.back(), std::optional<std::string>("b.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("a.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
    EXPECT_EQ(history.forward(), std::optional<std::string>("a.md"));
    EXPECT_EQ(history.forward(), std::optional<std::string>("b.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("a.md"));
    EXPECT_EQ(history.forward(), std::optional<std::string>("b.md"));
    EXPECT_EQ(history.forward(), std::optional<std::string>("c.md"));
    EXPECT_FALSE(history.can_go_forward());
    EXPECT_EQ(history.back(), std::optional<std::string>("b.md"));
}

TEST(PageHistoryForward, OpeningANewPageEmptiesTheForwardList) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.back();
    EXPECT_TRUE(history.can_go_forward());
    EXPECT_TRUE(history.open("b.md"));
    EXPECT_FALSE(history.can_go_forward());
    EXPECT_FALSE(history.forward().has_value());
    EXPECT_EQ(history.current(), std::string("b.md"));
    EXPECT_EQ(history.back(), std::optional<std::string>("index.md"));
    EXPECT_FALSE(history.can_go_back());
}

TEST(PageHistoryForward, OpeningTheCurrentPageOrNothingKeepsTheForwardList) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.back();
    EXPECT_FALSE(history.open("index.md"));
    EXPECT_FALSE(history.open(""));
    EXPECT_TRUE(history.can_go_forward());
    EXPECT_EQ(history.forward(), std::optional<std::string>("a.md"));
}

TEST(PageHistoryForward, AFailedBackChangesNothingAboutForward) {
    PageHistory history;
    history.open("index.md");
    history.open("a.md");
    history.back();
    EXPECT_FALSE(history.back().has_value());
    EXPECT_EQ(history.current(), std::string("index.md"));
    EXPECT_TRUE(history.can_go_forward());
    EXPECT_EQ(history.forward(), std::optional<std::string>("a.md"));
}

TEST(PageHistoryForward, ForwardCanRetraceTheWholeBackList) {
    PageHistory history;
    for (int i = 0; i < 150; ++i) {
        history.open("p" + std::to_string(i) + ".md");
    }
    std::size_t backs = 0;
    while (backs < 2 * PageHistory::kMaxBack && history.back()) {
        ++backs;
    }
    EXPECT_EQ(backs, PageHistory::kMaxBack);
    EXPECT_EQ(history.current(), std::string("p49.md"));
    std::size_t forwards = 0;
    while (forwards < 2 * PageHistory::kMaxBack && history.forward()) {
        ++forwards;
    }
    EXPECT_EQ(forwards, PageHistory::kMaxBack);
    EXPECT_EQ(history.current(), std::string("p149.md"));
    EXPECT_FALSE(history.can_go_forward());
}

} // namespace
