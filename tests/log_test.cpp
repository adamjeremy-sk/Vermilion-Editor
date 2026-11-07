// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the logging layer: domain, level mapping, redaction and format safety.

#include <glib.h>

#include <string>
#include <vector>

#include "core/log.h"
#include "gtest/gtest.h"

namespace {

struct Entry {
    std::string domain;
    GLogLevelFlags level;
    std::string message;
};

class CoreLog : public ::testing::Test {
protected:
    void SetUp() override {
        handler_id_ = g_log_set_handler_full(
            vermilion::logging::kDomain, G_LOG_LEVEL_MASK,
            [](const gchar* domain, GLogLevelFlags level, const gchar* message,
               gpointer user_data) {
                auto* entries = static_cast<std::vector<Entry>*>(user_data);
                entries->push_back(Entry{domain, level, message});
            },
            &captured_, nullptr);
    }

    void TearDown() override { g_log_remove_handler(vermilion::logging::kDomain, handler_id_); }

    std::vector<Entry> captured_;
    guint handler_id_ = 0;
};

TEST_F(CoreLog, DomainAndLevelWarning) {
    vermilion::logging::warning("hello");
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].domain, std::string("vermilion"));
    EXPECT_EQ(captured_[0].level, G_LOG_LEVEL_WARNING);
    EXPECT_EQ(captured_[0].message, std::string("hello"));
}

TEST_F(CoreLog, AllLevelsMapped) {
    vermilion::logging::debug("d");
    vermilion::logging::info("i");
    vermilion::logging::warning("w");
    vermilion::logging::error("e");
    ASSERT_EQ(captured_.size(), 4u);
    EXPECT_EQ(captured_[0].level, G_LOG_LEVEL_DEBUG);
    EXPECT_EQ(captured_[1].level, G_LOG_LEVEL_INFO);
    EXPECT_EQ(captured_[2].level, G_LOG_LEVEL_WARNING);
    EXPECT_EQ(captured_[3].level, G_LOG_LEVEL_CRITICAL);
}

TEST_F(CoreLog, SecretsRedacted) {
    vermilion::logging::warning("Authorization: Bearer abcdef123456");
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, std::string("Authorization: [REDACTED]"));
}

TEST_F(CoreLog, PercentSignsKept) {
    vermilion::logging::info("100% done %s %d");
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, std::string("100% done %s %d"));
}

TEST_F(CoreLog, EmptyMessage) {
    vermilion::logging::info("");
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, std::string(""));
}

} // namespace
