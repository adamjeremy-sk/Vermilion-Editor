// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The options of a search (text, case, whole word, regex) and the check of its pattern.

#pragma once

#include <optional>
#include <string>

namespace vermilion {

// What the Find and Replace windows hold. With regex off the search is literal: a backslash in the
// text is a backslash, never an escape.
struct SearchOptions {
    std::string text;
    bool match_case = false;
    bool whole_word = false;
    bool regex = false;
};

inline bool operator==(const SearchOptions& a, const SearchOptions& b) {
    return a.text == b.text && a.match_case == b.match_case && a.whole_word == b.whole_word &&
           a.regex == b.regex;
}

inline bool operator!=(const SearchOptions& a, const SearchOptions& b) {
    return !(a == b);
}

// True when there is something to search for: the text is not empty. (An empty text switches the
// search off; it is not an error.)
inline bool search_enabled(const SearchOptions& options) {
    return !options.text.empty();
}

// The text to hand to GtkSourceView's search. The text itself, except when regex and whole_word are
// both on and the text is not empty: then "(?:" + text + ")". GtkSourceView puts its word
// boundaries around the pattern as written, so with whole word on "foo|bar" matched the foo at the
// start of "foobar" (observed on GtkSourceView 5.12). A non-capturing group keeps the alternation
// inside the boundaries and leaves the group numbers of the pattern as they are.
std::string effective_search_text(const SearchOptions& options);

// Why the text is not a valid regular expression, as GLib words it, or nothing: nothing when
// regex is off, when the text is empty, and when the pattern compiles. The pattern is compiled
// with G_REGEX_MULTILINE, and with G_REGEX_CASELESS when match_case is false. When whole_word is
// on, effective_search_text() is compiled as well, because a pattern that compiles alone can fail
// inside the group (a trailing "# comment" under (?x) swallows the closing parenthesis). Exact
// wording is GLib's (and may be translated), so callers show it and never compare it.
std::optional<std::string> pattern_error(const SearchOptions& options);

} // namespace vermilion
