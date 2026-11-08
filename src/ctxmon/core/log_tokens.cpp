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
#include <vector>

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

std::vector<TokenMatch> token_matches(std::string_view text) {
    std::vector<TokenMatch> matches;
    if (text.empty()) {
        return matches; // also covers a default string_view, whose data() is null
    }
    // The pattern is constant, so compiling cannot fail; every test runs it.
    g_autoptr(GRegex) regex =
        g_regex_new(kTokenPattern, G_REGEX_RAW, G_REGEX_MATCH_DEFAULT, nullptr);
    g_autoptr(GMatchInfo) info = nullptr;
    g_regex_match_full(regex, text.data(), static_cast<gssize>(text.size()), 0,
                       G_REGEX_MATCH_DEFAULT, &info, nullptr);
    while (g_match_info_matches(info)) {
        gint match_start = -1;
        gint match_end = -1;
        g_match_info_fetch_pos(info, 0, &match_start, &match_end);
        for (const Group& group : kGroups) {
            gint start = -1;
            gint end = -1;
            if (g_match_info_fetch_named_pos(info, group.name, &start, &end) && start >= 0) {
                long long value = 0;
                const auto result = std::from_chars(text.data() + start, text.data() + end, value);
                if (result.ec == std::errc()) {
                    matches.push_back(
                        {static_cast<std::size_t>(match_start), TokenReading{value, group.kind}});
                }
            }
        }
        g_match_info_next(info, nullptr);
    }
    return matches;
}

std::optional<TokenReading> last_token(std::string_view text) {
    const std::vector<TokenMatch> matches = token_matches(text);
    if (matches.empty()) {
        return std::nullopt;
    }
    return matches.back().reading;
}

} // namespace vermilion::ctxmon
