// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Log token scanning: TOKEN_RE of the Python original on GRegex, and the tail window.

#include "ctxmon/core/log_tokens.h"

#include <glib.h>

#include <charconv>
#include <cstddef>
#include <optional>
#include <string_view>
#include <system_error>

namespace vermilion::ctxmon {

namespace {

// llama.cpp has used all three wordings over time.
constexpr const char* kTokenPattern = R"re(stop processing: n_tokens = (?P<final>\d+))re"
                                      R"re(|prompt processing done, n_tokens = (?P<prompt>\d+))re"
                                      R"re(|\bn_past = (?P<past>\d+))re";

struct Group {
    const char* name;
    TokenKind kind;
};

constexpr Group kGroups[] = {
    {"final", TokenKind::Final},
    {"prompt", TokenKind::Prompt},
    {"past", TokenKind::Past},
};

} // namespace

std::size_t tail_start(std::size_t file_size, std::size_t floor) {
    if (floor > file_size) {
        floor = 0;
    }
    const std::size_t tail = file_size > kTailBytes ? file_size - kTailBytes : 0;
    return floor > tail ? floor : tail;
}

std::optional<TokenReading> last_token(std::string_view text) {
    if (text.empty()) {
        return std::nullopt; // also covers a default string_view, whose data() is null
    }
    // The pattern is constant, so compiling cannot fail; every test runs it.
    g_autoptr(GRegex) regex =
        g_regex_new(kTokenPattern, G_REGEX_RAW, G_REGEX_MATCH_DEFAULT, nullptr);
    g_autoptr(GMatchInfo) info = nullptr;
    g_regex_match_full(regex, text.data(), static_cast<gssize>(text.size()), 0,
                       G_REGEX_MATCH_DEFAULT, &info, nullptr);
    std::optional<TokenReading> last;
    while (g_match_info_matches(info)) {
        for (const Group& group : kGroups) {
            gint start = -1;
            gint end = -1;
            if (g_match_info_fetch_named_pos(info, group.name, &start, &end) && start >= 0) {
                long long value = 0;
                const auto result = std::from_chars(text.data() + start, text.data() + end, value);
                if (result.ec == std::errc()) {
                    last = TokenReading{value, group.kind};
                }
            }
        }
        g_match_info_next(info, nullptr);
    }
    return last;
}

} // namespace vermilion::ctxmon
