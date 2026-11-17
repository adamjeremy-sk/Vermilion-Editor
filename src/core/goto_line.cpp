// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Go to Line: reading the typed text, fitting it to the document, and the texts around it.

#include "core/goto_line.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "core/count_text.h"

namespace vermilion {

namespace {

// A non-empty run of ASCII digits, read into a uint64 capped at 999999999 so a long run cannot
// overflow.
std::optional<std::uint64_t> read_number(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - '0');
        if (value > 999999999u) {
            value = 999999999u;
        }
    }
    return value;
}

} // namespace

std::optional<GotoRequest> parse_goto(const std::string& text) {
    std::size_t first = 0;
    std::size_t last = text.size();
    while (first < last && (text[first] == ' ' || text[first] == '\t')) {
        ++first;
    }
    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t')) {
        --last;
    }
    const std::string body = text.substr(first, last - first);
    if (body.empty()) {
        return std::nullopt;
    }
    const std::size_t colon = body.find(':');
    std::optional<std::uint64_t> line;
    std::uint64_t column = 0;
    if (colon == std::string::npos) {
        line = read_number(body);
    } else {
        line = read_number(body.substr(0, colon));
        std::optional<std::uint64_t> parsed_column = read_number(body.substr(colon + 1));
        if (!parsed_column.has_value()) {
            return std::nullopt;
        }
        column = *parsed_column;
    }
    if (!line.has_value() || *line == 0) {
        return std::nullopt;
    }
    if (colon != std::string::npos && column == 0) {
        return std::nullopt;
    }
    GotoRequest request;
    request.line = *line;
    request.column = column;
    return request;
}

GotoTarget resolve_goto(const GotoRequest& request, std::uint64_t line_count) {
    const std::uint64_t last = line_count == 0 ? 1u : line_count;
    GotoTarget target;
    if (request.line > last) {
        target.line = last;
        target.column = 0;
        target.past_end = true;
    } else {
        target.line = request.line;
        target.column = request.column;
        target.past_end = false;
    }
    return target;
}

std::string goto_range_text(std::uint64_t line_count) {
    const std::uint64_t last = line_count == 0 ? 1u : line_count;
    return "1 to " + with_commas(last);
}

std::string goto_past_end_message(const GotoRequest& request, std::uint64_t last_line) {
    return "Line " + with_commas(request.line) + " does not exist; went to the last line, " +
           with_commas(last_line) + ".";
}

} // namespace vermilion
