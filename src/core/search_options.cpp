// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Checks that the text of a regular expression search compiles, and says why when it does not.

#include "core/search_options.h"

#include <glib.h>

#include <memory>
#include <optional>
#include <string>

namespace vermilion {

namespace {

std::optional<std::string> compile_error(const std::string& text, bool match_case) {
    int flags = G_REGEX_MULTILINE;
    if (!match_case) {
        flags |= G_REGEX_CASELESS;
    }
    GError* raw = nullptr;
    g_autoptr(GRegex) regex = g_regex_new(text.c_str(), static_cast<GRegexCompileFlags>(flags),
                                          G_REGEX_MATCH_DEFAULT, &raw);
    if (regex != nullptr) {
        return std::nullopt;
    }
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (error == nullptr) {
        return std::string("Not a valid regular expression.");
    }
    return std::string(error->message);
}

} // namespace

std::string effective_search_text(const SearchOptions& options) {
    if (options.regex && options.whole_word && !options.text.empty()) {
        return "(?:" + options.text + ")";
    }
    return options.text;
}

std::optional<std::string> pattern_error(const SearchOptions& options) {
    if (!options.regex || options.text.empty()) {
        return std::nullopt;
    }
    std::optional<std::string> error = compile_error(options.text, options.match_case);
    if (error.has_value() || !options.whole_word) {
        return error;
    }
    return compile_error(effective_search_text(options), options.match_case);
}

} // namespace vermilion
