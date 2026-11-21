// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The colour mode: its text form and the decision between light and dark.

#include "core/colour_mode.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace vermilion {

namespace {

std::string lower_ascii(std::string_view text) {
    std::string result(text);
    for (char& c : result)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    return result;
}

std::string_view trim_blanks(std::string_view text) {
    const auto is_blank = [](char c) { return c == ' ' || c == '\t'; };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_blank(text[begin]))
        ++begin;
    while (end > begin && is_blank(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

bool ends_in_dark(const std::string& theme_name) {
    if (theme_name.size() <= 5)
        return false;
    const std::string lower = lower_ascii(theme_name);
    return lower.compare(lower.size() - 5, 5, "-dark") == 0;
}

} // namespace

const char* colour_mode_id(ColourMode mode) {
    switch (mode) {
    case ColourMode::Light:
        return "light";
    case ColourMode::Dark:
        return "dark";
    case ColourMode::System:
        return "system";
    }
    return "system";
}

ColourMode parse_colour_mode(std::string_view text) {
    const std::string lower = lower_ascii(trim_blanks(text));
    if (lower == "light")
        return ColourMode::Light;
    if (lower == "dark")
        return ColourMode::Dark;
    return ColourMode::System;
}

std::optional<bool> portal_dark_from_color_scheme(std::uint32_t value) {
    if (value == 1)
        return true;
    if (value == 2)
        return false;
    return std::nullopt;
}

bool resolve_dark(ColourMode mode, const SystemHints& hints) {
    switch (mode) {
    case ColourMode::Light:
        return false;
    case ColourMode::Dark:
        return true;
    case ColourMode::System:
        break;
    }
    if (hints.portal_dark.has_value())
        return *hints.portal_dark;
    if (ends_in_dark(hints.theme_name))
        return true;
    return hints.prefer_dark_at_start;
}

} // namespace vermilion
