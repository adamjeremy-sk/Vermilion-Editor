// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the inline spans of the manual subset: bold, code, topic links and plain text.

#include "core/manual_spans.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vermilion {

namespace {

constexpr std::size_t kMaxLinkText = 200;
constexpr std::size_t kMaxLinkTarget = 64;

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

struct Bracket {
    std::string_view text;
    std::string_view target;
    std::size_t end;
};

bool parse_bracket(std::string_view s, std::size_t open, Bracket& out) {
    std::string_view window = s.substr(open + 1, kMaxLinkText + 1);
    std::size_t close = window.find(']');
    if (close == std::string_view::npos) {
        return false;
    }
    std::string_view text = window.substr(0, close);
    if (text.find('[') != std::string_view::npos) {
        return false;
    }
    std::size_t paren = open + 1 + close + 1;
    if (paren >= s.size() || s[paren] != '(') {
        return false;
    }
    std::string_view rest = s.substr(paren + 1, kMaxLinkTarget + 1);
    std::size_t stop = rest.find(')');
    if (stop == std::string_view::npos) {
        return false;
    }
    out.text = text;
    out.target = rest.substr(0, stop);
    out.end = paren + 1 + stop + 1;
    return true;
}

class SpanParser {
public:
    SpanParser(std::string_view s, std::size_t& fallbacks) : s_(s), fallbacks_(fallbacks) {}

    std::vector<ManualSpan> run() {
        while (pos_ < s_.size()) {
            step();
        }
        flush();
        return std::move(spans_);
    }

private:
    void flush() {
        if (!plain_.empty()) {
            ManualSpan span;
            span.kind = SpanKind::Text;
            span.text = std::move(plain_);
            spans_.push_back(std::move(span));
        }
    }

    void add(SpanKind kind, std::string text, std::string target = std::string()) {
        flush();
        ManualSpan span;
        span.kind = kind;
        span.text = std::move(text);
        span.target = std::move(target);
        spans_.push_back(std::move(span));
    }

    void literal(std::string_view text) {
        plain_.append(text);
        ++fallbacks_;
    }

    bool starts_tag() const {
        if (pos_ + 1 >= s_.size()) {
            return false;
        }
        char next = s_[pos_ + 1];
        return s_[pos_] == '<' && (is_alpha(next) || next == '/' || next == '!');
    }

    void step() {
        char c = s_[pos_];
        if (c == '`') {
            code();
        } else if (c == '*') {
            star();
        } else if (c == '[') {
            bracket(pos_, pos_);
        } else if (c == '!' && pos_ + 1 < s_.size() && s_[pos_ + 1] == '[') {
            bracket(pos_ + 1, pos_);
        } else if (starts_tag()) {
            literal("<");
            ++pos_;
        } else {
            plain_.push_back(c);
            ++pos_;
        }
    }

    void code() {
        std::size_t end = s_.find('`', pos_ + 1);
        if (end == std::string_view::npos) {
            literal("`");
            ++pos_;
            return;
        }
        if (end == pos_ + 1) {
            literal("``");
            pos_ += 2;
            return;
        }
        add(SpanKind::Code, std::string(s_.substr(pos_ + 1, end - pos_ - 1)));
        pos_ = end + 1;
    }

    void star() {
        if (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '*') {
            literal("*");
            ++pos_;
            return;
        }
        std::size_t end = s_.find("**", pos_ + 2);
        if (end == std::string_view::npos) {
            literal("**");
            pos_ += 2;
            return;
        }
        if (end == pos_ + 2) {
            literal("****");
            pos_ += 4;
            return;
        }
        add(SpanKind::Bold, std::string(s_.substr(pos_ + 2, end - pos_ - 2)));
        pos_ = end + 2;
    }

    void bracket(std::size_t open, std::size_t start) {
        Bracket found;
        if (!parse_bracket(s_, open, found)) {
            plain_.append(s_.substr(start, open - start + 1));
            pos_ = open + 1;
            return;
        }
        if (start == open && !found.text.empty() && is_topic_name(found.target)) {
            add(SpanKind::Link, std::string(found.text), std::string(found.target));
        } else {
            literal(s_.substr(start, found.end - start));
        }
        pos_ = found.end;
    }

    std::string_view s_;
    std::size_t& fallbacks_;
    std::size_t pos_ = 0;
    std::string plain_;
    std::vector<ManualSpan> spans_;
};

} // namespace

bool is_topic_name(std::string_view name) {
    if (name.size() < 4 || name.size() > 64) {
        return false;
    }
    if (name.substr(name.size() - 3) != ".md") {
        return false;
    }
    std::string_view stem = name.substr(0, name.size() - 3);
    if (stem.empty() || stem.front() == '-') {
        return false;
    }
    for (char c : stem) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

std::vector<ManualSpan> parse_spans(std::string_view text, std::size_t& fallbacks) {
    return SpanParser(text, fallbacks).run();
}

} // namespace vermilion
