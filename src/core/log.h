// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Thin logging layer over GLib's g_log: every message is redacted before it is logged.

#pragma once

#include <string_view>

namespace vermilion::logging {

inline constexpr const char* kDomain = "vermilion";

void debug(std::string_view message);
void info(std::string_view message);
void warning(std::string_view message);
// Uses G_LOG_LEVEL_CRITICAL, never G_LOG_LEVEL_ERROR, because that level aborts the program.
void error(std::string_view message);

}  // namespace vermilion::logging
