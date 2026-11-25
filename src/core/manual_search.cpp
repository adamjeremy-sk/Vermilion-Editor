// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the search in a manual page: matches and stepping.

#include "core/manual_search.h"

#include <glib.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/manual_text.h"

namespace vermilion {

namespace {

// The regular expression class of word characters: letters, digits and underscore.
constexpr char kWordClass[] = "[\\p{L}\\p{N}_]";

// The pattern to compile: the search text, escaped when literal, bounded when whole word.
std::string full_pattern(const SearchOptions& options) {
    std::string pattern = options.text;
    if (!options.regex) {
        g_autofree gchar* escaped =
            g_regex_escape_string(options.text.c_str(), static_cast<gint>(options.text.size()));
        pattern = escaped;
    }
    if (options.whole_word) {
        pattern = std::string("(?<!") + kWordClass + ")(?:" + pattern + ")(?!" + kWordClass + ")";
    }
    return pattern;
}

// The number of characters between two byte offsets of a UTF-8 text.
std::size_t chars_between(std::string_view text, std::size_t from, std::size_t to) {
    return character_count(text.substr(from, to - from));
}

} // namespace

std::vector<SearchMatch> find_matches(std::string_view text, const SearchOptions& options) {
    std::vector<SearchMatch> matches;
    if (text.empty() || !search_enabled(options) || pattern_error(options).has_value() ||
        options.text.find('\0') != std::string::npos) {
        return matches;
    }
    int flags = G_REGEX_MULTILINE;
    if (!options.match_case) {
        flags |= G_REGEX_CASELESS;
    }
    g_autoptr(GRegex) regex =
        g_regex_new(full_pattern(options).c_str(), static_cast<GRegexCompileFlags>(flags),
                    G_REGEX_MATCH_DEFAULT, nullptr);
    if (regex == nullptr) {
        return matches;
    }
    GMatchInfo* raw = nullptr;
    g_regex_match_full(regex, text.data(), static_cast<gssize>(text.size()), 0,
                       G_REGEX_MATCH_DEFAULT, &raw, nullptr);
    std::unique_ptr<GMatchInfo, decltype(&g_match_info_free)> info(raw, &g_match_info_free);
    if (raw == nullptr) {
        return matches;
    }
    std::size_t byte = 0;
    std::size_t chars = 0;
    while (matches.size() < kMaxMatches && g_match_info_matches(info.get())) {
        gint start = 0;
        gint stop = 0;
        if (g_match_info_fetch_pos(info.get(), 0, &start, &stop) && stop > start) {
            const std::size_t begin = static_cast<std::size_t>(start);
            const std::size_t end = static_cast<std::size_t>(stop);
            const std::size_t first = chars + chars_between(text, byte, begin);
            const std::size_t last = first + chars_between(text, begin, end);
            matches.push_back({first, last});
            byte = end;
            chars = last;
        }
        if (!g_match_info_next(info.get(), nullptr)) {
            break;
        }
    }
    return matches;
}

std::optional<MatchStep> step_match(std::size_t count, std::optional<std::size_t> current,
                                    bool forward) {
    if (count == 0) {
        return std::nullopt;
    }
    if (!current.has_value() || *current >= count) {
        return MatchStep{forward ? 0 : count - 1, false};
    }
    if (forward) {
        return *current + 1 < count ? MatchStep{*current + 1, false} : MatchStep{0, true};
    }
    return *current > 0 ? MatchStep{*current - 1, false} : MatchStep{count - 1, true};
}

} // namespace vermilion
