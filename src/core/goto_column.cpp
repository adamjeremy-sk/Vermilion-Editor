// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line, the column part: which character of a line a typed column means.

#include "core/goto_column.h"

#include <cstdint>
#include <string>

namespace vermilion {

std::uint64_t goto_column_offset(const std::string& line_text, unsigned tab_width,
                                 std::uint64_t column) {
    const std::uint64_t width = tab_width == 0 ? 1 : tab_width;
    const std::uint64_t target = column == 0 ? 0 : column - 1;
    std::uint64_t visual = 0;
    std::uint64_t characters = 0;
    for (const char byte : line_text) {
        if ((static_cast<unsigned char>(byte) & 0xC0) == 0x80) {
            continue;
        }
        const std::uint64_t covers = byte == '\t' ? width - visual % width : 1;
        if (visual + covers > target) {
            return characters;
        }
        visual += covers;
        characters += 1;
    }
    return characters;
}

} // namespace vermilion
