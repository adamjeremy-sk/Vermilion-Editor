// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Formats monitor values for display, as the Python original does.

#pragma once

#include <string>

namespace vermilion::ctxmon {

// Formats a duration in seconds: "42s" below 90 seconds, otherwise "1m30s".
// Rounds halves to even, like Python's round().
std::string fmt_duration(double seconds);

} // namespace vermilion::ctxmon
