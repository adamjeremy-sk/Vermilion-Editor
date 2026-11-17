// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The status line texts of the Find and Replace windows.

#pragma once

#include <cstdint>
#include <string>

namespace vermilion {

// The match count line. position is the number of the current match counted from 1 (0 or less
// when the cursor is not on a match), count is the number of matches (negative while the buffer
// is still being scanned).
// count < 0: "Searching" followed by one ellipsis character (U+2026). count == 0: "Not found".
// A usable position (1 to count): "3 of 17 matches", "1 of 1 match".
// Otherwise: "17 matches", "1 match".
// Both numbers go through with_commas(): "1,204 of 15,310 matches".
std::string match_count_message(int position, int count);

// After a search ran past the end of the text: "Wrapped to the top" (forward) or "Wrapped to the
// bottom" (backward).
std::string wrapped_message(bool forward);

// After Replace All: "Nothing to replace" (0), "Replaced 1 match", "Replaced 42 matches"
// (through with_commas(): "Replaced 1,234 matches").
std::string replaced_message(std::uint64_t count);

// While a Replace All job runs: "Replacing: 1,204,000 of about 3,400,000" when total is above 0 and
// not below replaced, otherwise "Replacing: 1,204,000" (a total of 0 means unknown). Both numbers
// go through with_commas().
std::string replacing_message(std::uint64_t replaced, std::uint64_t total);

// After a Replace All job was cancelled: "Stopped, nothing was replaced" (0), "Stopped after
// replacing 1 match", "Stopped after replacing 1,234 matches".
std::string replace_stopped_message(std::uint64_t replaced);

// GLib words its errors "Error while compiling regular expression <q>(<q> at char 1: missing
// terminating )" (compiling) and "Error while parsing replacement text <q>\e<q> at char 1:
// unknown escape sequence" (replacements), where <q> is a typographic quote character. The reason
// is the text after the last " at char <digits>: " in the message; a message of another shape
// (the replacement rules of core/search_replacement.h have their own sentences) is returned
// unchanged.
std::string error_reason(const std::string& message);

// "Not a valid pattern: " + error_reason(message).
std::string pattern_problem_message(const std::string& message);

// "Not a valid replacement: " + the reason, when message has GLib's shape (error_reason() changes
// it); any other message is a sentence of our own and is returned unchanged.
std::string replacement_problem_message(const std::string& message);

} // namespace vermilion
