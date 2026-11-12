// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for manual_spans: bold, code, topic links, the constructs outside the subset, size limits.

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_spans.h"

namespace {

using vermilion::ManualSpan;
using vermilion::SpanKind;

// One span as T[text], B[text], C[text] or L[text]->target, spans separated by "|".
std::string describe(const std::vector<ManualSpan>& spans) {
    std::string out;
    for (const ManualSpan& span : spans) {
        if (!out.empty()) {
            out += '|';
        }
        switch (span.kind) {
        case SpanKind::Text:
            out += "T[";
            break;
        case SpanKind::Bold:
            out += "B[";
            break;
        case SpanKind::Code:
            out += "C[";
            break;
        case SpanKind::Link:
            out += "L[";
            break;
        }
        out += span.text;
        out += ']';
        if (span.kind == SpanKind::Link) {
            out += "->" + span.target;
        }
    }
    return out;
}

struct Parsed {
    std::string shape;
    std::size_t fallbacks = 0;
};

Parsed parse(const std::string& text) {
    Parsed parsed;
    parsed.shape = describe(vermilion::parse_spans(text, parsed.fallbacks));
    return parsed;
}

// The text must come out as one plain span, unchanged, with this many fallbacks.
void expect_plain(const std::string& text, std::size_t fallbacks) {
    SCOPED_TRACE(text);
    Parsed parsed = parse(text);
    EXPECT_EQ(parsed.shape, "T[" + text + "]");
    EXPECT_EQ(parsed.fallbacks, fallbacks);
}

TEST(ManualSpans, PlainTextIsOneSpan) {
    Parsed parsed = parse("Hello, world → “quoted”");
    EXPECT_EQ(parsed.shape, "T[Hello, world → “quoted”]");
    EXPECT_EQ(parsed.fallbacks, std::size_t{0});
    EXPECT_EQ(parse("").shape, "");
}

TEST(ManualSpans, BoldAndCode) {
    Parsed parsed = parse("a **b** c `d` e");
    EXPECT_EQ(parsed.shape, "T[a ]|B[b]|T[ c ]|C[d]|T[ e]");
    EXPECT_EQ(parsed.fallbacks, std::size_t{0});
    EXPECT_EQ(parse("**x**`y`").shape, "B[x]|C[y]");
}

TEST(ManualSpans, BoldTextIsLiteralAndCodeShieldsMarkup) {
    EXPECT_EQ(parse("**a `b` c**").shape, "B[a `b` c]");
    Parsed parsed = parse("`**not bold** [a](b.md) <x> *`");
    EXPECT_EQ(parsed.shape, "C[**not bold** [a](b.md) <x> *]");
    EXPECT_EQ(parsed.fallbacks, std::size_t{0});
}

TEST(ManualSpans, TopicLink) {
    Parsed parsed = parse("see [Opening and saving files](open-and-save.md).");
    EXPECT_EQ(parsed.shape, "T[see ]|L[Opening and saving files]->open-and-save.md|T[.]");
    EXPECT_EQ(parsed.fallbacks, std::size_t{0});
    EXPECT_EQ(parse("[a](b.md)[c](d.md)").shape, "L[a]->b.md|L[c]->d.md");
}

TEST(ManualSpans, UnclosedBoldStaysAsWritten) {
    expect_plain("x **y", 1);
    expect_plain("**", 1);
    expect_plain("a **** b", 1);
}

TEST(ManualSpans, UnclosedOrEmptyCodeStaysAsWritten) {
    expect_plain("x `y", 1);
    expect_plain("a `` b", 1);
}

TEST(ManualSpans, LinksThatAreNotTopicsStayAsWritten) {
    expect_plain("[docs](https://example.org)", 1);
    expect_plain("[x](no.txt) [y](Index.md) [z](a/b.md) [w](../a.md)", 4);
    expect_plain("[](a.md)", 1);
    expect_plain("[x](a.md#part)", 1);
}

TEST(ManualSpans, ImagesStayAsWritten) {
    expect_plain("![logo](logo.png) x", 1);
    expect_plain("![logo](a.md)", 1);
}

TEST(ManualSpans, LooseBracketsAreOrdinaryText) {
    expect_plain("[x] and [1](", 0);
    expect_plain("a ! b ![x] c", 0);
    expect_plain("[a](b.md", 0);
    Parsed inner = parse("[a [b](c.md)");
    EXPECT_EQ(inner.shape, "T[[a ]|L[b]->c.md");
    EXPECT_EQ(inner.fallbacks, std::size_t{0});
}

TEST(ManualSpans, LoneStarAndTagsAreFallbacks) {
    expect_plain("a * b", 1);
    expect_plain("x <b> y", 1);
    expect_plain("a </x", 1);
    expect_plain("<!-- c -->", 1);
    expect_plain("1 < 2 and 3 > 2", 0);
    expect_plain("a <\u00e9 b", 0);
    expect_plain("back\\slash", 0);
}

TEST(ManualSpans, TextAroundFallbacksIsMerged) {
    Parsed parsed = parse("a * b **c** d * e");
    EXPECT_EQ(parsed.shape, "T[a * b ]|B[c]|T[ d * e]");
    EXPECT_EQ(parsed.fallbacks, std::size_t{2});
}

TEST(ManualSpans, LinkSizeLimits) {
    // The link text may have 200 bytes, the target 64 characters.
    std::string text200(200, 'x');
    std::string text201(201, 'x');
    std::string name64 = std::string(61, 'a') + ".md";
    std::string name65 = std::string(62, 'a') + ".md";
    EXPECT_EQ(parse("[" + text200 + "](a.md)").shape, "L[" + text200 + "]->a.md");
    expect_plain("[" + text201 + "](a.md)", 0);
    EXPECT_EQ(parse("[t](" + name64 + ")").shape, "L[t]->" + name64);
    expect_plain("[t](" + name65 + ")", 0);
}

TEST(ManualSpans, TopicNames) {
    EXPECT_TRUE(vermilion::is_topic_name("index.md"));
    EXPECT_TRUE(vermilion::is_topic_name("open-and-save.md"));
    EXPECT_TRUE(vermilion::is_topic_name("a.md"));
    EXPECT_TRUE(vermilion::is_topic_name("2fa-setup.md"));
    EXPECT_TRUE(vermilion::is_topic_name(std::string(61, 'a') + ".md"));
    EXPECT_FALSE(vermilion::is_topic_name(""));
    EXPECT_FALSE(vermilion::is_topic_name(".md"));
    EXPECT_FALSE(vermilion::is_topic_name("md"));
    EXPECT_FALSE(vermilion::is_topic_name("a.md.md"));
    EXPECT_FALSE(vermilion::is_topic_name("-a.md"));
    EXPECT_FALSE(vermilion::is_topic_name("A.md"));
    EXPECT_FALSE(vermilion::is_topic_name("a b.md"));
    EXPECT_FALSE(vermilion::is_topic_name("a/b.md"));
    EXPECT_FALSE(vermilion::is_topic_name("../a.md"));
    EXPECT_FALSE(vermilion::is_topic_name("a.MD"));
    EXPECT_FALSE(vermilion::is_topic_name("a.txt"));
    EXPECT_FALSE(vermilion::is_topic_name("a.md\n"));
    EXPECT_FALSE(vermilion::is_topic_name("é.md"));
    EXPECT_FALSE(vermilion::is_topic_name(std::string(62, 'a') + ".md"));
    EXPECT_FALSE(vermilion::is_topic_name(std::string("a\0b.md", 6)));
}

// A megabyte of hostile text must not take more than a moment or lose a byte.
void expect_fast_and_whole(const std::string& text, std::size_t min_fallbacks) {
    auto start = std::chrono::steady_clock::now();
    std::size_t fallbacks = 0;
    std::vector<ManualSpan> spans = vermilion::parse_spans(text, fallbacks);
    auto elapsed = std::chrono::steady_clock::now() - start;
    std::size_t shown = 0;
    for (const ManualSpan& span : spans) {
        shown += span.text.size();
    }
    EXPECT_EQ(shown, text.size());
    EXPECT_GE(fallbacks, min_fallbacks);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 3000);
}

TEST(ManualSpans, HostileMegabyteInputs) {
    const std::size_t mib = 1024 * 1024;
    expect_fast_and_whole(std::string(mib, '['), 0);
    expect_fast_and_whole(std::string(mib, '*'), 1);
    expect_fast_and_whole(std::string(mib, '`'), 1);
    expect_fast_and_whole(std::string(mib, '<') + "a", 1);
    std::string brackets;
    while (brackets.size() < mib) {
        brackets += "[a](";
    }
    expect_fast_and_whole(brackets, 0);
    std::string unclosed;
    while (unclosed.size() < mib) {
        unclosed += "[a";
    }
    expect_fast_and_whole(unclosed + "]", 0);
}

TEST(ManualSpans, NulAndHighBytesPassThrough) {
    std::string text("a\0b\xff\xfe", 5);
    std::size_t fallbacks = 0;
    std::vector<ManualSpan> spans = vermilion::parse_spans(text, fallbacks);
    ASSERT_EQ(spans.size(), std::size_t{1});
    EXPECT_EQ(spans[0].text, text);
    EXPECT_EQ(fallbacks, std::size_t{0});
}

} // namespace
