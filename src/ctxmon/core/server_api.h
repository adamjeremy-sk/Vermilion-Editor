// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Parses the llama.cpp server's /slots, /props and /v1/models response bodies.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vermilion::ctxmon {

// What the first slot of GET /slots reports. Every field is absent when the body is not JSON,
// not a non-empty array, or its first element is not an object (the Python original treats
// those as "the server answered, but without slot data").
struct SlotInfo {
    std::optional<long long> n_ctx;
    std::optional<bool> is_processing;
    std::optional<long long> n_decoded; // from next_token, which is an object or a list of them
};

SlotInfo parse_slots(std::string_view body);

// The model name in a GET /props body: model_alias if it is a non-empty string, otherwise the
// file name of model_path. Absent when neither gives a non-empty name or the body is not a
// JSON object. A member that is not a string counts as missing.
std::optional<std::string> model_from_props(std::string_view body);

// The id of the first model in a GET /v1/models body (may be empty, as in the Python original),
// or absent when there is none, it is not a string, or the body is not JSON.
std::optional<std::string> model_from_models(std::string_view body);

} // namespace vermilion::ctxmon
