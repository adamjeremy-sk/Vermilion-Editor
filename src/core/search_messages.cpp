// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The status line texts of the Find and Replace windows.

#include "core/search_messages.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/count_text.h"

namespace vermilion {

namespace {

std::string matches_text(std::uint64_t count) {
    return with_commas(count) + (count == 1u ? " match" : " matches");
}

} // namespace

std::string match_count_message(int position, int count) {
    if (count < 0) {
        return "Searching\u2026";
    }
    if (count == 0) {
        return "Not found";
    }
    const std::uint64_t total = static_cast<std::uint64_t>(count);
    if (position >= 1 && position <= count) {
        return with_commas(static_cast<std::uint64_t>(position)) + " of " + matches_text(total);
    }
    return matches_text(total);
}

std::string wrapped_message(bool forward) {
    return forward ? "Wrapped to the top" : "Wrapped to the bottom";
}

std::string replaced_message(std::uint64_t count) {
    if (count == 0) {
        return "Nothing to replace";
    }
    return "Replaced " + matches_text(count);
}

std::string replacing_message(std::uint64_t replaced, std::uint64_t total) {
    std::string text = "Replacing: " + with_commas(replaced);
    if (total > 0 && replaced <= total) {
        text += " of about " + with_commas(total);
    }
    return text;
}

std::string replace_stopped_message(std::uint64_t replaced) {
    if (replaced == 0) {
        return "Stopped, nothing was replaced";
    }
    return "Stopped after replacing " + matches_text(replaced);
}

std::string error_reason(const std::string& message) {
    const std::string marker = " at char ";
    const std::size_t at = message.rfind(marker);
    if (at == std::string::npos) {
        return message;
    }
    std::size_t i = at + marker.size();
    std::size_t digits = 0;
    while (i < message.size() && message[i] >= '0' && message[i] <= '9') {
        ++i;
        ++digits;
    }
    if (digits == 0) {
        return message;
    }
    if (i + 2 > message.size() || message.compare(i, 2, ": ") != 0) {
        return message;
    }
    const std::size_t reason_start = i + 2;
    if (reason_start >= message.size()) {
        return message;
    }
    return message.substr(reason_start);
}

std::string pattern_problem_message(const std::string& message) {
    return "Not a valid pattern: " + error_reason(message);
}

std::string replacement_problem_message(const std::string& message) {
    const std::string reason = error_reason(message);
    if (reason == message) {
        return message;
    }
    return "Not a valid replacement: " + reason;
}

} // namespace vermilion
