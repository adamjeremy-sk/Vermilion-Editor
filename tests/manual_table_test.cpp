// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for manual_table: the column widths and the layout of a table as padded, wrapped text.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_table.h"
#include "core/manual_text.h"

namespace {

using vermilion::TableLayout;
using vermilion::TextRange;

// The page width these tests lay out for. It is fixed here, so they do not follow a change of
// the width of the Help page (kManualColumns).
constexpr std::size_t kColumns = 80;

// The layout of the first table in `markdown`, in lines of kColumns characters.
TableLayout lay(const std::string& markdown) {
    const vermilion::ManualPage page = vermilion::parse_manual(markdown);
    for (const vermilion::ManualBlock& block : page.blocks) {
        if (block.kind == vermilion::ManualBlockKind::Table) {
            return vermilion::layout_table(block, kColumns);
        }
    }
    return TableLayout{};
}

// The characters of `text` from offset `begin` up to but not including `end`.
std::string slice(const std::string& text, std::size_t begin, std::size_t end) {
    std::string out;
    std::size_t number = 0; // the 1-based number of the character the byte belongs to
    for (const char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++number;
        }
        if (number > begin && number <= end) {
            out += c;
        }
    }
    return out;
}

std::string words_of(const TableLayout& layout, const TextRange& range) {
    return slice(layout.text, range.begin, range.end);
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines(1);
    for (const char c : text) {
        if (c == '\n') {
            lines.emplace_back();
        } else {
            lines.back() += c;
        }
    }
    return lines;
}

TEST(ManualTableLayout, ColumnsArePaddedAndJoined) {
    const TableLayout layout =
        lay("| Keys | Command |\n|---|---|\n| Ctrl+N | New File |\n| Ctrl+S | Save |");
    EXPECT_EQ(layout.text, std::string("Keys   | Command \n"
                                       "-------+---------\n"
                                       "Ctrl+N | New File\n"
                                       "Ctrl+S | Save    "));
    EXPECT_EQ(layout.length, std::size_t{71});
    EXPECT_EQ(layout.header_end, std::size_t{17});
    EXPECT_TRUE(layout.bold.empty());
    EXPECT_TRUE(layout.code.empty());
    EXPECT_TRUE(layout.links.empty());
}

TEST(ManualTableLayout, AShortRowGetsEmptyCells) {
    const TableLayout layout = lay("| A | B |\n|---|---|\n| only |");
    EXPECT_EQ(layout.text, std::string("A    | B\n-----+--\nonly |  "));
}

TEST(ManualTableLayout, CellsCountCharactersNotBytes) {
    const TableLayout layout = lay("| \xC3\xA9 | x |\n|---|---|\n| abc | y |");
    EXPECT_EQ(layout.text, std::string("\xC3\xA9   | x\n----+--\nabc | y"));
    EXPECT_EQ(layout.length, std::size_t{23});
}

TEST(ManualTableLayout, RunsOfBlanksBecomeOneSpace) {
    const TableLayout layout = lay("| a   b | c |\n|---|---|\n| d | e |");
    EXPECT_EQ(layout.text, std::string("a b | c\n----+--\nd   | e"));
}

TEST(ManualTableLayout, ALongCellWrapsInsideItsColumn) {
    const TableLayout layout = lay(
        "| Name | Description |\n|---|---|\n| a | one two three four five six seven eight nine ten "
        "eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty |\n"
        "| `bc` | **bold words that go on** and then `code span` end |");
    const std::vector<std::string> lines = lines_of(layout.text);
    ASSERT_EQ(lines.size(), std::size_t{5});
    for (const std::string& line : lines) {
        EXPECT_EQ(line.size(), std::size_t{80});
    }
    EXPECT_EQ(lines[2], "a    | one two three four five six seven eight nine ten eleven twelve "
                        "thirteen  ");
    EXPECT_EQ(lines[3], "     | fourteen fifteen sixteen seventeen eighteen nineteen twenty     "
                        "         ");
    EXPECT_EQ(lines[4].substr(0, 28), "bc   | bold words that go on");
    EXPECT_EQ(layout.header_end, std::size_t{80});
    ASSERT_EQ(layout.bold.size(), std::size_t{1});
    EXPECT_EQ(words_of(layout, layout.bold[0]), "bold words that go on");
    ASSERT_EQ(layout.code.size(), std::size_t{2});
    EXPECT_EQ(words_of(layout, layout.code[0]), "bc");
    EXPECT_EQ(words_of(layout, layout.code[1]), "code span");
}

TEST(ManualTableLayout, ALinkInACellKeepsItsTarget) {
    const TableLayout layout = lay("| A | B |\n|---|---|\n| see [more](a-topic.md) | y |");
    ASSERT_EQ(layout.links.size(), std::size_t{1});
    EXPECT_EQ(words_of(layout, {layout.links[0].begin, layout.links[0].end}), "more");
    EXPECT_EQ(layout.links[0].target, "a-topic.md");
}

TEST(ManualTableLayout, AStyleSurvivesAWrapAndTheSpaceBeforeItHasNone) {
    std::string row = "| k | ";
    for (int i = 0; i < 30; ++i) {
        row += "x ";
    }
    row += "**";
    for (int i = 0; i < 10; ++i) {
        row += "y ";
    }
    row += "y** |";
    const TableLayout layout = lay("| K | V |\n|---|---|\n" + row);
    ASSERT_EQ(lines_of(layout.text).size(), std::size_t{4});
    ASSERT_EQ(layout.bold.size(), std::size_t{2});
    EXPECT_EQ(words_of(layout, layout.bold[0]), "y y y y y y y y");
    EXPECT_EQ(words_of(layout, layout.bold[1]), "y y y");
}

TEST(ManualTableLayout, AWrappedHeaderIsAllHeader) {
    std::string header = "| A | ";
    for (int i = 1; i <= 40; ++i) {
        header += "h" + std::to_string(i) + " ";
    }
    const TableLayout layout = lay(header + "|\n|---|---|\n| a | b |");
    const std::vector<std::string> lines = lines_of(layout.text);
    ASSERT_EQ(lines.size(), std::size_t{4});
    EXPECT_EQ(lines[2].find_first_not_of("-+"), std::string::npos);
    EXPECT_EQ(layout.header_end, std::size_t{161});
}

TEST(ManualTableLayout, ALongWordIsCutAtTheColumnWidth) {
    const std::string word(100, 'x');
    const TableLayout layout = lay("| A | B |\n|---|---|\n| " + word + " | y |");
    const std::vector<std::string> lines = lines_of(layout.text);
    ASSERT_EQ(lines.size(), std::size_t{4});
    EXPECT_EQ(lines[2], std::string(76, 'x') + " | y");
    EXPECT_EQ(lines[3], std::string(24, 'x') + std::string(52, ' ') + " |  ");
}

TEST(ManualTableLayout, TooManyColumnsOverflowWithColumnsOfOneCharacter) {
    std::string md;
    for (int i = 0; i < 21; ++i) {
        md += "| a ";
    }
    md += "|\n";
    for (int i = 0; i < 21; ++i) {
        md += "|---";
    }
    md += "|\n";
    EXPECT_EQ(lines_of(lay(md).text)[0].size(), std::size_t{81});
}

TEST(ManualTableLayout, ASmallerWidthWrapsEarlier) {
    const vermilion::ManualPage page =
        vermilion::parse_manual("| A | B |\n|---|---|\n| a | one two three four |");
    const TableLayout layout = vermilion::layout_table(page.blocks[0], 14);
    EXPECT_EQ(layout.text, "A | B" + std::string(9, ' ') + "\n--+" + std::string(11, '-') +
                               "\na | one two   \n  | three four");
}

using Widths = std::vector<std::size_t>;

TEST(ManualTableColumns, NaturalWidthsThatFitAreKept) {
    EXPECT_EQ(vermilion::fit_columns({5, 5}, {5, 5}, 80), (Widths{5, 5}));
    EXPECT_EQ(vermilion::fit_columns({38, 38}, {3, 3}, 80), (Widths{38, 38}));
    EXPECT_EQ(vermilion::fit_columns({7}, {7}, 7), (Widths{7}));
}

TEST(ManualTableColumns, TheWidestColumnTakesTheSpareRoom) {
    EXPECT_EQ(vermilion::fit_columns({12, 22, 81, 22}, {12, 22, 10, 9}, 80),
              (Widths{12, 22, 28, 9}));
}

TEST(ManualTableColumns, TiesGoToTheLeftmostColumn) {
    EXPECT_EQ(vermilion::fit_columns({39, 39}, {10, 10}, 80), (Widths{39, 38}));
}

TEST(ManualTableColumns, WordsAreCutOnlyWhenNoWidthsHoldThem) {
    EXPECT_EQ(vermilion::fit_columns({30, 30, 30}, {30, 30, 30}, 40), (Widths{12, 11, 11}));
}

TEST(ManualTableColumns, AMinimumLargerThanTheNaturalWidthIsCut) {
    EXPECT_EQ(vermilion::fit_columns({4, 90}, {40, 20}, 80), (Widths{4, 73}));
}

TEST(ManualTableColumns, ZeroesCountAsOne) {
    EXPECT_EQ(vermilion::fit_columns({0, 0}, {0, 0}, 80), (Widths{1, 1}));
}

TEST(ManualTableColumns, NoColumnsAndTooManyColumns) {
    EXPECT_TRUE(vermilion::fit_columns({}, {}, 80).empty());
    EXPECT_EQ(vermilion::fit_columns(Widths(21, 5), Widths(21, 5), 80), Widths(21, 1));
}

} // namespace
