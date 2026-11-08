// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Display formatting of monitor values: durations.

#include "ctxmon/core/format.h"

#include <cmath>
#include <string>

namespace vermilion::ctxmon {

std::string fmt_duration(double seconds) {
    // nearbyint rounds halves to even in the default rounding mode, as Python's round() does.
    const long long s = static_cast<long long>(std::nearbyint(seconds));
    if (s < 90) {
        return std::to_string(s) + "s";
    }
    const long long rest = s % 60;
    return std::to_string(s / 60) + "m" + (rest < 10 ? "0" : "") + std::to_string(rest) + "s";
}

} // namespace vermilion::ctxmon
