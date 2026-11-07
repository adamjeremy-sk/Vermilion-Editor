// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implementation of the gtkx string conversions.

#include "ui/gtkx/string.h"

#include <glib.h>

namespace vermilion::gtkx {

std::string from_utf8(const char* s) {
    if (s == nullptr) {
        return {};
    }
    return std::string(s);
}

std::string take_string(char* s) {
    std::string result;
    if (s != nullptr) {
        result = s;
        g_free(s);
    }
    return result;
}

} // namespace vermilion::gtkx
