// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The result rows of a search over the manual: the snippet of a match and one row per page.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/manual_search.h"
#include "core/search_options.h"

namespace vermilion {

// The width in characters of a snippet, not counting its two ellipsis characters.
inline constexpr std::size_t kSnippetColumns = 60;

// A one-line piece of `text` around `match` for a list of results, in characters:
//  - Only the line of the match is used: the line that holds match.begin, from after the
//    previous "\n" (or the start) to the next "\n" (or the end). A match that runs past the end
//    of that line is cut at the end of the line. A match that begins at a "\n" gives "".
//  - A match of kSnippetColumns characters or more: the window is its first kSnippetColumns
//    characters. Otherwise the window has kSnippetColumns characters (fewer when the line is
//    shorter) with the match in it, with (kSnippetColumns - length) / 2 characters (integer
//    division) of the line before the match; when the line has fewer before it, the window starts
//    at the start of the line; when the window then runs past the end of the line it is moved left
//    as far as the start of the line allows.
//  - Inside the window every run of spaces and tabs becomes one space, and spaces at both ends
//    are removed. If nothing is left the result is "".
//  - U+2026 (one ellipsis character, "\xE2\x80\xA6") is put before the result when the window
//    starts after the start of the line, and after it when the window ends before the end of the
//    line.
// A `match` that is empty or reaches past the end of `text` gives "".
std::string match_snippet(std::string_view text, const SearchMatch& match);

// A page to search: its topic file, its title (the tree's row) and its text (ManualText::text).
struct SearchPage {
    std::string file;
    std::string title;
    std::string text;
};

// One row of the results: a page with at least one match.
struct PageResult {
    std::string file;
    std::string title;
    std::size_t count = 0; // find_matches().size() for the page
    std::string snippet;   // match_snippet() of the first match
};

// The pages that have a match, in the order given. Nothing when options do not search (see
// find_matches).
std::vector<PageResult> search_pages(const std::vector<SearchPage>& pages,
                                     const SearchOptions& options);

} // namespace vermilion
