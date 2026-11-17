// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The rules a replacement text must meet before Replace or Replace All runs.

#pragma once

#include <optional>
#include <string>

#include "core/search_options.h"

namespace vermilion {

// Why replacement cannot be used with options, in words for the status line, or nothing when it
// can. Rules, in this order:
// - With regex off the replacement is literal text: everything is allowed, backslashes included.
// - With regex on, an empty search text or a pattern that does not compile (pattern_error) means
//   nothing is judged here: the pattern's own error is the one to show.
// - GLib's syntax check (g_regex_check_replacement) comes first; its error text is returned as it
//   is. GLib accepts \0 to \9, \g<number>, \g<name>, \n \t \r \a \f \v \b, \xHH, \x{HHHH}, \\ and
//   the case switches \U \L \u \l \E, and rejects other escapes, a stray final backslash and a
//   broken \g<...>.
// - A backslash followed by two or more digits is refused, because GLib reads it as a group
//   number or as an octal code depending on its value: "A backslash followed by two or more
//   digits is ambiguous: use \g<12> for a group, or \x41 for a character."
// - A group the pattern does not have is refused, because GLib turns it into an empty string and
//   Replace All would silently delete text. Group 0 (the whole match) always exists. The pattern's
//   group count is g_regex_get_capture_count(); named groups count too.
//   By number (\1 to \9 or \g<number>), with count groups in the pattern:
//   "The replacement uses group N, but the pattern has no groups." (count 0), "... has only 1
//   group." (count 1), "... has only C groups." (count 2 or more).
//   By name (\g<name>), looked up with g_regex_get_string_number(): "The replacement names the
//   group " + quoted(name) + ", but the pattern has no group with that name." (quoted() is in
//   core/open_messages.h; it wraps the name in curly double quotes).
// The first problem found, reading the replacement from the left, is the one returned.
std::optional<std::string> replacement_error(const SearchOptions& options,
                                             const std::string& replacement);

} // namespace vermilion
