// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Conversions from GLib C strings to std::string, copying or taking ownership.

#pragma once

#include <string>

namespace vermilion::gtkx {

std::string from_utf8(const char* s);

std::string take_string(char* s);

}  // namespace vermilion::gtkx
