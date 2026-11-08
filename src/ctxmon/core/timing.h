// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reads llama.cpp timing lines: the latest speeds and the list of finished requests.

#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace vermilion::ctxmon {

// Speeds of the last timing lines in a piece of log text, in tokens per second.
struct LastSpeeds {
    std::optional<double> gen_tps;    // last "eval time" line (generation)
    std::optional<double> prompt_tps; // last "prompt eval time" line
};

// One finished request, built from its timing lines.
struct FinishedRequest {
    std::optional<long long> prompt_tokens; // absent without a preceding prompt line
    std::optional<double> prompt_tps;
    long long gen_tokens = 0;
    double gen_tps = 0.0;
    std::optional<long long> ctx; // context size after the request, when the log says so
};

// last_speeds of the Python original. A line whose numbers do not parse is skipped (the
// Python original would raise).
LastSpeeds last_speeds(std::string_view text);

// parse_requests of the Python original: finished requests in text, oldest first. Lines whose
// numbers do not parse are skipped, as in last_speeds.
std::vector<FinishedRequest> parse_requests(std::string_view text);

} // namespace vermilion::ctxmon
