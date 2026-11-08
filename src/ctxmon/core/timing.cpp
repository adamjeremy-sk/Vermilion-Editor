// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Timing-line parsing: GEN_RE and PROMPT_RE of the Python original on GRegex.

#include "ctxmon/core/timing.h"

#include <glib.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "ctxmon/core/log_tokens.h"

namespace vermilion::ctxmon {

namespace {

// "eval time = 1234.56 ms / 100 tokens ( 12.35 ms per token, 81.00 tokens per second)".
// Groups: 1 milliseconds, 2 token count, 3 tokens per second. The lookbehind keeps the
// generation pattern out of "prompt eval time" lines.
constexpr const char* kGenPattern =
    R"re((?<!prompt )eval time =\s*([\d.]+) ms /\s*(\d+) (?:tokens|runs)\s*)re"
    R"re(\(\s*[\d.]+ ms per token,\s*([\d.]+) tokens per second\))re";
constexpr const char* kPromptPattern =
    R"re(prompt eval time =\s*([\d.]+) ms /\s*(\d+) (?:tokens|runs)\s*)re"
    R"re(\(\s*[\d.]+ ms per token,\s*([\d.]+) tokens per second\))re";

// One timing line and the byte offset where its match starts.
struct Timing {
    std::size_t pos = 0;
    long long tokens = 0;
    double tps = 0.0;
};

// Parses capture group `group` into value; false if it is unset or not entirely a number.
template <typename T>
bool parse_group(GMatchInfo* info, gint group, std::string_view text, T& value) {
    gint start = -1;
    gint end = -1;
    if (!g_match_info_fetch_pos(info, group, &start, &end) || start < 0) {
        return false;
    }
    const char* last = text.data() + end;
    const auto result = std::from_chars(text.data() + start, last, value);
    return result.ec == std::errc() && result.ptr == last;
}

// Every match of pattern in text whose token count and speed parse, oldest first.
std::vector<Timing> timings(std::string_view text, const char* pattern) {
    std::vector<Timing> out;
    if (text.empty()) {
        return out;
    }
    // The patterns are constant, so compiling cannot fail; every test runs them.
    g_autoptr(GRegex) regex = g_regex_new(pattern, G_REGEX_RAW, G_REGEX_MATCH_DEFAULT, nullptr);
    g_autoptr(GMatchInfo) info = nullptr;
    g_regex_match_full(regex, text.data(), static_cast<gssize>(text.size()), 0,
                       G_REGEX_MATCH_DEFAULT, &info, nullptr);
    while (g_match_info_matches(info)) {
        gint start = -1;
        gint end = -1;
        g_match_info_fetch_pos(info, 0, &start, &end);
        Timing timing;
        timing.pos = static_cast<std::size_t>(start);
        if (parse_group(info, 2, text, timing.tokens) && parse_group(info, 3, text, timing.tps)) {
            out.push_back(timing);
        }
        g_match_info_next(info, nullptr);
    }
    return out;
}

enum class EventKind { Token, Prompt, Gen };

// A token line or a timing line, ordered by position for parse_requests.
struct Event {
    std::size_t pos = 0;
    EventKind kind = EventKind::Token;
    TokenKind token_kind = TokenKind::Final;
    long long tokens = 0;
    double tps = 0.0;
};

std::vector<Event> events_in(std::string_view text) {
    std::vector<Event> events;
    for (const TokenMatch& m : token_matches(text)) {
        events.push_back({m.pos, EventKind::Token, m.reading.kind, m.reading.tokens, 0.0});
    }
    for (const Timing& t : timings(text, kPromptPattern)) {
        events.push_back({t.pos, EventKind::Prompt, TokenKind::Final, t.tokens, t.tps});
    }
    for (const Timing& t : timings(text, kGenPattern)) {
        events.push_back({t.pos, EventKind::Gen, TokenKind::Final, t.tokens, t.tps});
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const Event& a, const Event& b) { return a.pos < b.pos; });
    return events;
}

} // namespace

LastSpeeds last_speeds(std::string_view text) {
    LastSpeeds speeds;
    const std::vector<Timing> gen = timings(text, kGenPattern);
    const std::vector<Timing> prompt = timings(text, kPromptPattern);
    if (!gen.empty()) {
        speeds.gen_tps = gen.back().tps;
    }
    if (!prompt.empty()) {
        speeds.prompt_tps = prompt.back().tps;
    }
    return speeds;
}

std::vector<FinishedRequest> parse_requests(std::string_view text) {
    std::vector<FinishedRequest> out;
    std::optional<Event> pending; // the last prompt line not yet paired with a generation line
    bool awaiting_ctx = false;
    for (const Event& e : events_in(text)) {
        if (e.kind == EventKind::Token) {
            // llama.cpp prints the timings first and the release line right after, so the
            // first non-prompt token line after a generation line is that request's context.
            if (awaiting_ctx && e.token_kind != TokenKind::Prompt) {
                out.back().ctx = e.tokens;
                awaiting_ctx = false;
            }
        } else if (e.kind == EventKind::Prompt) {
            pending = e;
        } else {
            FinishedRequest request;
            if (pending) {
                request.prompt_tokens = pending->tokens;
                request.prompt_tps = pending->tps;
            }
            request.gen_tokens = e.tokens;
            request.gen_tps = e.tps;
            out.push_back(request);
            pending.reset();
            awaiting_ctx = true;
        }
    }
    return out;
}

} // namespace vermilion::ctxmon
