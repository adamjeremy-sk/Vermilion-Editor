// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/result.h"

using vermilion::Error;
using vermilion::Result;
using vermilion::Status;
using vermilion::Unit;
using vermilion::ok_status;

TEST(CoreResult, OkHoldsValue) {
    auto r = Result<int>::ok(5);
    EXPECT_TRUE(r.is_ok());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.value(), 5);
}

TEST(CoreResult, ErrHoldsMessage) {
    auto r = Result<int>::err(Error{"boom"});
    EXPECT_FALSE(r.is_ok());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.error().message, std::string("boom"));
}

TEST(CoreResult, ValueOrReturnsFallbackOnError) {
    auto r = Result<int>::err(Error{"err"});
    EXPECT_EQ(r.value_or(42), 42);
}

TEST(CoreResult, ValueOrReturnsValueOnSuccess) {
    auto r = Result<int>::ok(7);
    EXPECT_EQ(r.value_or(42), 7);
}

TEST(CoreResult, ConstAccess) {
    const Result<std::string> r = Result<std::string>::ok(std::string("hello"));
    EXPECT_EQ(r.value(), std::string("hello"));
}

TEST(CoreResult, MoveOnlyValueCanBeTaken) {
    auto r = Result<std::unique_ptr<int>>::ok(std::make_unique<int>(9));
    auto p = std::move(r).value();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(*p, 9);
}

TEST(CoreResult, StatusOkAndErr) {
    auto s = ok_status();
    EXPECT_TRUE(s.is_ok());
    auto e = Status::err(Error{"x"});
    EXPECT_FALSE(e.is_ok());
    EXPECT_EQ(e.error().message, std::string("x"));
}

TEST(CoreResult, ValueOnErrorAborts) {
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    EXPECT_DEATH(Result<int>::err(Error{"e"}).value(),
                 "Result::value\\(\\) called on an error");
}

TEST(CoreResult, ErrorOnSuccessAborts) {
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    EXPECT_DEATH(Result<int>::ok(1).error(),
                 "Result::error\\(\\) called on a success");
}
