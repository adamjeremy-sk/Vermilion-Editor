// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the result rows of a search over the manual.

#include "core/manual_results.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/manual_search.h"

namespace vermilion {

namespace {

// The byte offset where each character of `text` starts, in order, with `text.size()` at the end.
std::vector<std::size_t> character_starts(std::string_view text) {
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            starts.push_back(i);
        }
    }
    starts.push_back(text.size());
    return starts;
}

// `piece` with every run of spaces and tabs as one space and no space at either end.
std::string squeeze(const std::string& piece) {
    std::string out;
    bool pending = false;
    for (char c : piece) {
        if (c == ' ' || c == '\t') {
            pending = !out.empty();
        } else {
            if (pending) {
                out += ' ';
                pending = false;
            }
            out += c;
        }
    }
    return out;
}

} // namespace

std::string match_snippet(std::string_view text, const SearchMatch& match) {
    const std::vector<std::size_t> starts = character_starts(text);
    const std::size_t length = starts.size() - 1;
    if (match.begin >= match.end || match.end > length) {
        return std::string();
    }
    const auto is_newline = [&](std::size_t i) { return text[starts[i]] == '\n'; };
    std::size_t line_begin = match.begin;
    while (line_begin > 0 && !is_newline(line_begin - 1)) {
        --line_begin;
    }
    std::size_t line_end = match.begin;
    while (line_end < length && !is_newline(line_end)) {
        ++line_end;
    }
    const std::size_t match_end = std::min(match.end, line_end);
    if (match_end <= match.begin) {
        return std::string();
    }
    const std::size_t width = match_end - match.begin;
    std::size_t begin = match.begin;
    if (width < kSnippetColumns) {
        const std::size_t before = (kSnippetColumns - width) / 2;
        begin = match.begin - line_begin >= before ? match.begin - before : line_begin;
    }
    const std::size_t end = std::min(begin + kSnippetColumns, line_end);
    if (end - begin < kSnippetColumns) {
        begin = end - line_begin > kSnippetColumns ? end - kSnippetColumns : line_begin;
    }
    const std::string body =
        squeeze(std::string(text.substr(starts[begin], starts[end] - starts[begin])));
    if (body.empty()) {
        return body;
    }
    return std::string(begin > line_begin ? "\xE2\x80\xA6" : "") + body +
           (end < line_end ? "\xE2\x80\xA6" : "");
}

std::vector<PageResult> search_pages(const std::vector<SearchPage>& pages,
                                     const SearchOptions& options) {
    std::vector<PageResult> results;
    for (const SearchPage& page : pages) {
        const std::vector<SearchMatch> matches = find_matches(page.text, options);
        if (!matches.empty()) {
            results.push_back(
                {page.file, page.title, matches.size(), match_snippet(page.text, matches[0])});
        }
    }
    return results;
}

} // namespace vermilion
