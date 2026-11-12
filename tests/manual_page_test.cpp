// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for manual_page: headings, paragraphs, lists, tables, the constructs outside the subset.

#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_page.h"

namespace {

using vermilion::ManualBlock;
using vermilion::ManualBlockKind;
using vermilion::ManualPage;
using vermilion::ManualSpan;
using vermilion::SpanKind;

// The shown text of spans, with the kind of every non-plain span marked as in the spans test.
std::string flat(const std::vector<ManualSpan>& spans) {
    std::string out;
    for (const ManualSpan& span : spans) {
        switch (span.kind) {
        case SpanKind::Text:
            out += span.text;
            break;
        case SpanKind::Bold:
            out += "<b>" + span.text + "</b>";
            break;
        case SpanKind::Code:
            out += "<c>" + span.text + "</c>";
            break;
        case SpanKind::Link:
            out += "<a " + span.target + ">" + span.text + "</a>";
            break;
        }
    }
    return out;
}

// One block as H1[text], P[text], I[text] or a table T[row;row] with cells separated by "|".
std::string describe(const ManualBlock& block) {
    switch (block.kind) {
    case ManualBlockKind::Heading:
        return "H" + std::to_string(block.level) + "[" + flat(block.spans) + "]";
    case ManualBlockKind::Paragraph:
        return "P[" + flat(block.spans) + "]";
    case ManualBlockKind::ListItem:
        return "I[" + flat(block.spans) + "]";
    case ManualBlockKind::Table:
        break;
    }
    std::string out = "T[";
    for (std::size_t r = 0; r < block.rows.size(); ++r) {
        out += r == 0 ? "" : ";";
        for (std::size_t c = 0; c < block.rows[r].size(); ++c) {
            out += c == 0 ? "" : "|";
            out += flat(block.rows[r][c]);
        }
    }
    return out + "]";
}

// The page as its blocks joined with "\n".
std::string describe(const ManualPage& page) {
    std::string out;
    for (const ManualBlock& block : page.blocks) {
        out += out.empty() ? "" : "\n";
        out += describe(block);
    }
    return out;
}

TEST(ManualPage, EmptyAndBlankInput) {
    EXPECT_TRUE(vermilion::parse_manual("").blocks.empty());
    ManualPage blank = vermilion::parse_manual("\n\n  \n\t\n \r\n");
    EXPECT_TRUE(blank.blocks.empty());
    EXPECT_EQ(blank.fallbacks, std::size_t{0});
}

TEST(ManualPage, Headings) {
    ManualPage page = vermilion::parse_manual("# One\n## Two  \n###### Six\n");
    EXPECT_EQ(describe(page), "H1[One]\nH2[Two]\nH6[Six]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, NotHeadings) {
    // None of these lines is a heading, so they all join into one paragraph.
    ManualPage page = vermilion::parse_manual("####### Seven\n#tag\n#\n # x\n");
    EXPECT_EQ(describe(page), "P[####### Seven #tag # # x]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, HeadingCarriesSpans) {
    ManualPage page = vermilion::parse_manual("# The **big** `one`\n");
    EXPECT_EQ(describe(page), "H1[The <b>big</b> <c>one</c>]");
}

TEST(ManualPage, ParagraphJoinsLines) {
    ManualPage page = vermilion::parse_manual("first line\nsecond  line \n   third\n\nnext");
    EXPECT_EQ(describe(page), "P[first line second  line third]\nP[next]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, ALinkMayBreakAcrossLines) {
    ManualPage page = vermilion::parse_manual("see [Opening and\nsaving](open-and-save.md).\n");
    EXPECT_EQ(describe(page), "P[see <a open-and-save.md>Opening and saving</a>.]");
}

TEST(ManualPage, ParagraphEndsAtHeadingListAndTable) {
    ManualPage page =
        vermilion::parse_manual("text\n# H\ntext2\n- item\ntext3\n| a |\n|---|\n| b |\n");
    EXPECT_EQ(describe(page), "P[text]\nH1[H]\nP[text2]\nI[item]\nP[text3]\nT[a;b]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, ListItemsAndContinuations) {
    ManualPage page =
        vermilion::parse_manual("- one\n- two\n  continues here\n   and here\n- three\n\n- four\n");
    EXPECT_EQ(describe(page), "I[one]\nI[two continues here and here]\nI[three]\nI[four]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, ListItemNeedsAMarkerAndText) {
    ManualPage page = vermilion::parse_manual("-x\n-\n");
    EXPECT_EQ(describe(page), "P[-x -]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
    page = vermilion::parse_manual("- a\n b\n");
    EXPECT_EQ(describe(page), "I[a]\nP[b]");
}

TEST(ManualPage, NestedListIsAFallback) {
    ManualPage page = vermilion::parse_manual("- a\n  - b\n  - c\n- d\n");
    EXPECT_EQ(describe(page), "I[a - b - c]\nI[d]");
    EXPECT_EQ(page.fallbacks, std::size_t{2});
}

TEST(ManualPage, Table) {
    ManualPage page = vermilion::parse_manual(
        "| Keys | Command |\n|---|:-:|\n| Ctrl+N | New File |\n| **Alt** | `x` |\n");
    ASSERT_EQ(page.blocks.size(), std::size_t{1});
    EXPECT_EQ(describe(page), "T[Keys|Command;Ctrl+N|New File;<b>Alt</b>|<c>x</c>]");
    EXPECT_EQ(page.blocks[0].rows.size(), std::size_t{3});
    EXPECT_EQ(page.blocks[0].columns, std::size_t{2});
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, TableWithoutTrailingPipesAndWithLinks) {
    ManualPage page = vermilion::parse_manual("| a | b\n|---|---\n| [x](y.md) | 2\n");
    EXPECT_EQ(describe(page), "T[a|b;<a y.md>x</a>|2]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, TableEndsAtABlankLineOrOtherText) {
    ManualPage page = vermilion::parse_manual("| a |\n|---|\n| 1 |\ntext\n\n| b |\n|:--|\n");
    EXPECT_EQ(describe(page), "T[a;1]\nP[text]\nT[b]");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, RaggedRowsAreKeptAndCounted) {
    ManualPage page = vermilion::parse_manual(
        "| a | b | c |\n|---|---|---|\n| 1 | 2 |\n| 1 | 2 | 3 | 4 |\n| x | y | z |\n");
    ASSERT_EQ(page.blocks.size(), std::size_t{1});
    EXPECT_EQ(describe(page), "T[a|b|c;1|2;1|2|3|4;x|y|z]");
    EXPECT_EQ(page.blocks[0].columns, std::size_t{4});
    EXPECT_EQ(page.blocks[0].rows[1].size(), std::size_t{2});
    EXPECT_EQ(page.fallbacks, std::size_t{2});
}

TEST(ManualPage, PipeLinesThatAreNoTable) {
    ManualPage page = vermilion::parse_manual("| a | b |\n| 1 | 2 |\n");
    EXPECT_EQ(describe(page), "P[| a | b |]\nP[| 1 | 2 |]");
    EXPECT_EQ(page.fallbacks, std::size_t{2});
    // A separator with another number of cells does not make a table either.
    page = vermilion::parse_manual("| a | b |\n|---|\n| 1 |\n");
    EXPECT_EQ(describe(page), "P[| a | b |]\nP[|---|]\nP[| 1 |]");
    EXPECT_EQ(page.fallbacks, std::size_t{3});
    // Neither does a separator row with an empty cell.
    page = vermilion::parse_manual("| a |\n| |\n");
    EXPECT_EQ(describe(page), "P[| a |]\nP[| |]");
    page = vermilion::parse_manual("| a |\n|:|\n");
    EXPECT_EQ(describe(page), "P[| a |]\nP[|:|]");
}

TEST(ManualPage, QuotesAreFallbacksAndStayAsWritten) {
    ManualPage page = vermilion::parse_manual("> quoted\n> more\n");
    EXPECT_EQ(describe(page), "P[> quoted > more]");
    EXPECT_EQ(page.fallbacks, std::size_t{2});
}

TEST(ManualPage, FencesAreFallbacksAndStayAsWritten) {
    ManualPage page = vermilion::parse_manual("~~~\ncode\n~~~\n");
    EXPECT_EQ(describe(page), "P[~~~ code ~~~]");
    EXPECT_EQ(page.fallbacks, std::size_t{2});
    // Backtick fences count twice for the two fence lines and twice more for the empty pair of
    // backticks at each end of the joined text, which the span parser sees.
    page = vermilion::parse_manual("```\ncode\n```\n");
    ASSERT_EQ(page.blocks.size(), std::size_t{1});
    EXPECT_EQ(page.fallbacks, std::size_t{4});
}

TEST(ManualPage, NumberedAndPlusListsAreFallbacks) {
    ManualPage page = vermilion::parse_manual("1. one\n2) two\n10. ten\n");
    EXPECT_EQ(describe(page), "P[1. one 2) two 10. ten]");
    EXPECT_EQ(page.fallbacks, std::size_t{3});
    page = vermilion::parse_manual("+ plus\n");
    EXPECT_EQ(describe(page), "P[+ plus]");
    EXPECT_EQ(page.fallbacks, std::size_t{1});
    page = vermilion::parse_manual("1.5 is a number\n2024 was a year\n");
    EXPECT_EQ(page.fallbacks, std::size_t{0});
}

TEST(ManualPage, RulesAreFallbacks) {
    ManualPage page = vermilion::parse_manual("---\n");
    EXPECT_EQ(describe(page), "P[---]");
    EXPECT_EQ(page.fallbacks, std::size_t{1});
    EXPECT_EQ(vermilion::parse_manual("--\n").fallbacks, std::size_t{0});
    EXPECT_EQ(vermilion::parse_manual("-----\n").fallbacks, std::size_t{1});
}

TEST(ManualPage, FallbacksInSpansAreCounted) {
    ManualPage page =
        vermilion::parse_manual("# a *\n\ntext **b\n\n- [x](y.png)\n\n| <p> |\n|---|\n");
    EXPECT_EQ(describe(page), "H1[a *]\nP[text **b]\nI[[x](y.png)]\nT[<p>]");
    EXPECT_EQ(page.fallbacks, std::size_t{4});
}

TEST(ManualPage, CrLfAndNoFinalNewline) {
    ManualPage page = vermilion::parse_manual("# T\r\n\r\ntext\r\nmore\r\n- a\r\n- b");
    EXPECT_EQ(describe(page), "H1[T]\nP[text more]\nI[a]\nI[b]");
    EXPECT_EQ(vermilion::parse_manual("text").blocks.size(), std::size_t{1});
}

// Letters and digits of a text, in order.
std::string alnum(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            out += c;
        }
    }
    return out;
}

std::string shown_letters(const ManualPage& page) {
    std::string out;
    for (const ManualBlock& block : page.blocks) {
        for (const ManualSpan& span : block.spans) {
            out += span.text;
        }
        for (const vermilion::ManualRow& row : block.rows) {
            for (const vermilion::ManualCell& cell : row) {
                for (const ManualSpan& span : cell) {
                    out += span.text;
                }
            }
        }
    }
    return alnum(out);
}

// Whatever the subset does not understand is still shown: no letter or digit goes missing.
TEST(ManualPage, NothingIsDropped) {
    const char* const inputs[] = {
        "> q1 *b* <c> `x ~~~ + y | z [q](https://e.org) ![i](p.png) **u",
        "1. a\n+ b\n---\n``\n****\n[](c.md)\n[d](e.txt)\n<f>\n",
        "| a | b |\n|-|\n| c |\n- x\n  - y\n",
        "**one\n\n`two\n\n[three](four\n\n![five]\n",
    };
    for (const char* input : inputs) {
        SCOPED_TRACE(input);
        EXPECT_EQ(shown_letters(vermilion::parse_manual(input)), alnum(input));
    }
}

TEST(ManualPage, NulAndHighBytesPassThrough) {
    std::string text("a\0b\n\xff\xfe\n", 7);
    ManualPage page = vermilion::parse_manual(text);
    ASSERT_EQ(page.blocks.size(), std::size_t{1});
    ASSERT_EQ(page.blocks[0].spans.size(), std::size_t{1});
    EXPECT_EQ(page.blocks[0].spans[0].text, std::string("a\0b \xff\xfe", 6));
}

// Big and odd inputs finish within a moment and keep all their text.
TEST(ManualPage, HostileInputs) {
    const std::size_t mib = 1024 * 1024;
    auto start = std::chrono::steady_clock::now();
    ManualPage line = vermilion::parse_manual(std::string(mib, 'a'));
    ASSERT_EQ(line.blocks.size(), std::size_t{1});
    ASSERT_EQ(line.blocks[0].spans.size(), std::size_t{1});
    EXPECT_EQ(line.blocks[0].spans[0].text.size(), mib);
    std::string items;
    while (items.size() < mib) {
        items += "- x\n";
    }
    EXPECT_EQ(vermilion::parse_manual(items).blocks.size(), mib / 4);
    EXPECT_TRUE(vermilion::parse_manual(std::string(mib, '\n')).blocks.empty());
    std::string wide = "|";
    std::string rule = "|";
    for (int i = 0; i < 10000; ++i) {
        wide += "a|";
        rule += "-|";
    }
    ManualPage table = vermilion::parse_manual(wide + "\n" + rule + "\n" + wide + "\n");
    ASSERT_EQ(table.blocks.size(), std::size_t{1});
    EXPECT_EQ(table.blocks[0].columns, std::size_t{10000});
    EXPECT_EQ(table.blocks[0].rows.size(), std::size_t{2});
    EXPECT_EQ(table.fallbacks, std::size_t{0});
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5000);
}

} // namespace
