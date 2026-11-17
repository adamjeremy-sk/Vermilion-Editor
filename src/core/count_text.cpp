// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A count in decimal, with a comma between every group of three digits.

#include "core/count_text.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace vermilion {

namespace {

std::string with_commas_text(std::uint64_t n) {
    const std::string digits = std::to_string(n);
    std::string result;
    result.reserve(digits.size() + digits.size() / 3);
    const int total = static_cast<int>(digits.size());
    for (int i = 0; i < total; ++i) {
        if (i != 0 && (total - i) % 3 == 0) {
            result.push_back(',');
        }
        result.push_back(digits[static_cast<std::size_t>(i)]);
    }
    return result;
}

} // namespace

std::string with_commas(std::uint64_t n) {
    return with_commas_text(n);
}

} // namespace vermilion
