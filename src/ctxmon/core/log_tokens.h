// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Finds the newest context-token count in llama.cpp server log text.

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace vermilion::ctxmon {

// How much of the log's end is scanned per poll (TAIL_BYTES in the Python original).
constexpr std::size_t kTailBytes = 512 * 1024;

// Which log wording reported the count.
enum class TokenKind {
    Final,  // "stop processing: n_tokens = N": the request finished
    Prompt, // "prompt processing done, n_tokens = N": generation is starting
    Past,   // older "n_past = N": progress or final; the caller decides from the busy state
};

struct TokenReading {
    long long tokens = 0;
    TokenKind kind = TokenKind::Final;
};

// One token line and the byte offset where its match starts in the text.
struct TokenMatch {
    std::size_t pos = 0;
    TokenReading reading;
};

// Byte offset where the scan of a log of file_size bytes starts. Content before floor (the
// session start) is ignored; a floor beyond the end means the log was truncated (server
// restart), so it counts as 0. At most the last kTailBytes are scanned.
std::size_t tail_start(std::size_t file_size, std::size_t floor);

// Every token line in text, oldest first. Bytes are matched raw, so a multi-byte character
// cut at the start of a tail does no harm. A number too large for long long is skipped (the
// Python original has no limit).
std::vector<TokenMatch> token_matches(std::string_view text);

// The last token line in text, or nothing if there is none (same rules as token_matches).
std::optional<TokenReading> last_token(std::string_view text);

} // namespace vermilion::ctxmon
