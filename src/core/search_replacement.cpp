// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Checks a replacement text against GLib's syntax and the groups of the pattern.

#include "core/search_replacement.h"

#include <cctype>
#include <cstddef>
#include <glib.h>
#include <memory>
#include <string>

#include "core/open_messages.h"

namespace vermilion {

namespace {

std::string group_number_message(long long group, int count) {
    std::string message =
        "The replacement uses group " + std::to_string(group) + ", but the pattern has ";
    if (count == 0) {
        message += "no groups.";
    } else if (count == 1) {
        message += "only 1 group.";
    } else {
        message += "only " + std::to_string(count) + " groups.";
    }
    return message;
}

std::string group_name_message(const std::string& name) {
    return "The replacement names the group " + quoted(name) +
           ", but the pattern has no group with that name.";
}

const char kAmbiguity[] =
    "A backslash followed by two or more digits is ambiguous: use \\g<12> for a group, or "
    "\\x41 for a character.";

bool all_digits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (const char digit : text) {
        if (!std::isdigit(static_cast<unsigned char>(digit))) {
            return false;
        }
    }
    return true;
}

long long group_number(const std::string& digits) {
    long long number = 0;
    for (const char digit : digits) {
        number = number * 10 + (digit - '0');
        if (number > 999999999) {
            return 999999999;
        }
    }
    return number;
}

std::optional<std::string> symbolic_error(const GRegex* regex, int count, const std::string& name) {
    if (all_digits(name)) {
        const long long number = group_number(name);
        if (number > count) {
            return group_number_message(number, count);
        }
        return std::nullopt;
    }
    const int group = g_regex_get_string_number(regex, name.c_str());
    if (group < 0) {
        return group_name_message(name);
    }
    return std::nullopt;
}

std::optional<std::string> references_error(const std::string& replacement, const GRegex* regex) {
    const int count = g_regex_get_capture_count(regex);
    const std::size_t n = replacement.size();
    for (std::size_t i = 0; i < n;) {
        const char c = replacement[i];
        if (c != '\\') {
            i += 1;
            continue;
        }
        if (i + 1 >= n) {
            i += 1;
            continue;
        }
        const char next = replacement[i + 1];
        if (next == 'g') {
            if (i + 2 >= n || replacement[i + 2] != '<') {
                return std::nullopt;
            }
            const std::size_t close = replacement.find('>', i + 3);
            if (close == std::string::npos) {
                return std::nullopt;
            }
            const std::string name = replacement.substr(i + 3, close - (i + 3));
            std::optional<std::string> error = symbolic_error(regex, count, name);
            if (error.has_value()) {
                return error;
            }
            i = close + 1;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(next))) {
            if (i + 2 < n && std::isdigit(static_cast<unsigned char>(replacement[i + 2]))) {
                return std::string(kAmbiguity);
            }
            const int group = next - '0';
            if (group > count) {
                return group_number_message(group, count);
            }
            i += 2;
            continue;
        }
        i += 2;
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> replacement_error(const SearchOptions& options,
                                             const std::string& replacement) {
    if (!options.regex || options.text.empty() || pattern_error(options).has_value()) {
        return std::nullopt;
    }

    GError* raw = nullptr;
    if (!g_regex_check_replacement(replacement.c_str(), nullptr, &raw)) {
        std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
        if (error == nullptr) {
            return std::string("Not a valid replacement.");
        }
        return std::string(error->message);
    }

    int flags = G_REGEX_MULTILINE;
    if (!options.match_case) {
        flags |= G_REGEX_CASELESS;
    }
    g_autoptr(GRegex) regex =
        g_regex_new(options.text.c_str(), static_cast<GRegexCompileFlags>(flags),
                    G_REGEX_MATCH_DEFAULT, nullptr);
    if (regex == nullptr) {
        return std::nullopt;
    }
    return references_error(replacement, regex);
}

} // namespace vermilion
