// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A count written for people: thousands separated by commas.

#pragma once

#include <cstdint>
#include <string>

namespace vermilion {

// n in decimal with a comma between thousands: 0, 999, 1,000, 12,408, 1,234,567. The comma is
// always used, whatever the locale (the interface is English only).
std::string with_commas(std::uint64_t n);

} // namespace vermilion
