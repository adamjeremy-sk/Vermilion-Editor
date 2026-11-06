// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include <gtest/gtest.h>

#include <string>

#include "agent/agent_info.h"

TEST(AgentInfo, ReturnsExpectedAgentName) {
    EXPECT_EQ(std::string(vermilion::agent_name()), "vermilion-agent");
}
