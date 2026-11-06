// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implementation of redact_secrets: scans left to right applying rules A, B and C.

#include "core/redact.h"

#include <cstddef>
#include <string_view>

namespace vermilion {
namespace {

constexpr std::string_view kRedacted = "[REDACTED]";

bool is_ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alnum(char c) {
    return is_ascii_letter(c) || is_ascii_digit(c);
}

char to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool ci_eq(char a, char b) {
    return to_lower_ascii(a) == to_lower_ascii(b);
}

bool is_space_tab(char c) {
    return c == ' ' || c == '\t';
}

bool is_quote(char c) {
    return c == '"' || c == '\'';
}

bool is_word_start(const std::string_view& s, std::size_t i) {
    return i == 0 || !is_alnum(s[i - 1]);
}

bool match_name(const std::string_view& s, std::size_t i, std::string_view name) {
    if (i + name.size() > s.size())
        return false;
    for (std::size_t k = 0; k < name.size(); ++k)
        if (!ci_eq(s[i + k], name[k]))
            return false;
    return true;
}

// A rule match: text[i, keep_end) is kept unchanged, [REDACTED] is appended,
// and scanning resumes at resume.
struct RuleMatch {
    bool matched = false;
    std::size_t keep_end = 0;
    std::size_t resume = 0;
};

// Rule A: named field.
RuleMatch try_rule_a(const std::string_view& s, std::size_t i) {
    static const std::string_view names[] = {
        "api_key", "apikey", "api-key", "password", "passwd", "secret", "token", "authorization"};
    std::string_view name;
    for (std::string_view n : names) {
        if (match_name(s, i, n)) {
            name = n;
            break;
        }
    }
    if (name.empty())
        return RuleMatch{};
    const bool is_auth = (name == "authorization");
    std::size_t p = i + name.size();
    if (p < s.size() && is_quote(s[p]))
        ++p;
    while (p < s.size() && is_space_tab(s[p]))
        ++p;
    if (p >= s.size() || (s[p] != ':' && s[p] != '='))
        return RuleMatch{};
    ++p;
    while (p < s.size() && is_space_tab(s[p]))
        ++p;
    if (p < s.size() && is_quote(s[p]))
        ++p;
    const std::size_t value_start = p;
    while (p < s.size()) {
        const char c = s[p];
        const bool stop = is_auth
                              ? (is_quote(c) || c == ',' || c == '\n' || c == '\r')
                              : (c == ' ' || c == '\t' || c == '\n' || c == '\r' || is_quote(c) ||
                                 c == ',' || c == ';' || c == '&' || c == '}');
        if (stop)
            break;
        ++p;
    }
    if (p == value_start)
        return RuleMatch{};
    return RuleMatch{true, value_start, p};
}

// Rule B: bearer token.
RuleMatch try_rule_b(const std::string_view& s, std::size_t i) {
    if (!match_name(s, i, "bearer"))
        return RuleMatch{};
    std::size_t p = i + 6;
    if (p >= s.size() || s[p] != ' ')
        return RuleMatch{};
    while (p < s.size() && s[p] == ' ')
        ++p;
    const std::size_t run_start = p;
    while (p < s.size()) {
        const char c = s[p];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' ||
            is_quote(c) || c == ',' || c == ';')
            break;
        ++p;
    }
    if (p - run_start < 8)
        return RuleMatch{};
    return RuleMatch{true, run_start, p};
}

bool is_token_char(char c) {
    return is_alnum(c) || c == '_' || c == '-';
}

// Rule C: key prefix; the whole token is replaced.
RuleMatch try_rule_c(const std::string_view& s, std::size_t i) {
    auto try_prefix = [&](std::string_view prefix, std::size_t min_len) -> std::size_t {
        if (s.compare(i, prefix.size(), prefix) != 0)
            return 0;
        std::size_t p = i + prefix.size();
        while (p < s.size() && is_token_char(s[p]))
            ++p;
        if (p - i < min_len)
            return 0;
        return p;
    };
    std::size_t end = try_prefix("sk-", 19);
    if (end == 0)
        end = try_prefix("AIza", 34);
    if (end == 0)
        return RuleMatch{};
    return RuleMatch{true, i, end};
}

}  // namespace

std::string redact_secrets(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        RuleMatch m;
        if (is_word_start(text, i)) {
            m = try_rule_a(text, i);
            if (!m.matched)
                m = try_rule_b(text, i);
            if (!m.matched)
                m = try_rule_c(text, i);
        }
        if (m.matched) {
            out.append(text.substr(i, m.keep_end - i));
            out.append(kRedacted);
            i = m.resume;
        } else {
            out.push_back(text[i]);
            ++i;
        }
    }
    return out;
}

}  // namespace vermilion
