// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Search in the text of a manual page: the matches of a search and stepping from match to match.

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "core/search_options.h"

namespace vermilion {

// The most matches of one page that are reported; the rest are ignored.
inline constexpr std::size_t kMaxMatches = 10000;

// A match: the characters from `begin` up to but not including `end` (offsets in characters, the
// unit of ManualText and of GtkTextBuffer, not bytes); never empty.
struct SearchMatch {
    std::size_t begin = 0;
    std::size_t end = 0;
};

inline bool operator==(const SearchMatch& a, const SearchMatch& b) {
    return a.begin == b.begin && a.end == b.end;
}

// All matches of `options` in `text` (valid UTF-8), in order, none overlapping, at most
// kMaxMatches. The search runs with GLib's GRegex and G_REGEX_MULTILINE ("^" and "$" match at
// every line):
//  - Nothing is found when the search text is empty, when it holds a NUL byte, when `text` is
//    empty, when `text` is not valid UTF-8, or when pattern_error(options) has a value.
//  - With regex off the search text is literal (g_regex_escape_string); with regex on it is the
//    pattern.
//  - With match_case off the match ignores case (G_REGEX_CASELESS), also for non-ASCII letters.
//  - With whole_word on, the character before a match and the character after it must not be a
//    word character (a letter, a digit or "_": \p{L}, \p{N}, "_"), or must not exist. Whole word
//    is done as "(?<![\p{L}\p{N}_])(?:" + pattern + ")(?![\p{L}\p{N}_])", so "foo|bar" never
//    matches inside "foobar".
//  - A match with no characters (a regular expression that matches the empty string) is skipped.
std::vector<SearchMatch> find_matches(std::string_view text, const SearchOptions& options);

// The match to go to after the one at `current`: a place in a list of `count` matches.
struct MatchStep {
    std::size_t index = 0;
    bool wrapped = false; // the step went past the last match (forward) or the first (backward)
};

// Steps forward or backward from the match numbered `current` (counted from 0) in a list of
// `count` matches. Nothing when `count` is 0. Without a current match (`current` empty, or not
// below `count`) the step goes to the first match (forward) or the last (backward) and is not
// wrapped. Forward from the last match wraps to 0, backward from 0 wraps to count - 1.
std::optional<MatchStep> step_match(std::size_t count, std::optional<std::size_t> current,
                                    bool forward);

} // namespace vermilion
