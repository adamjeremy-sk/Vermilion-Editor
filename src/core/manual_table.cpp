// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Lays out a manual table as padded text with wrapped cells.

#include "core/manual_table.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace vermilion {

namespace {

// A glyph is one character of a cell: the bytes of one code point and the index of the span it
// came from (-1 cast to size_t when it is a separator or padding, which belongs to no span).
struct Glyph {
    std::string bytes;
    std::size_t span = static_cast<std::size_t>(-1);
};

// A word is a run of glyphs between blanks; a line is a run of glyphs on one laid-out line.
using Word = std::vector<Glyph>;
using Line = std::vector<Glyph>;

// The words of a cell: the characters of all its spans in order, split at spaces and tabs.
std::vector<Word> cell_words(const ManualCell& cell) {
    std::vector<Word> words;
    Word current;
    for (std::size_t si = 0; si < cell.size(); ++si) {
        for (const char ch : cell[si].text) {
            const unsigned char b = static_cast<unsigned char>(ch);
            if ((b & 0xC0) == 0x80) {
                if (!current.empty()) {
                    current.back().bytes += ch; // a continuation byte joins the previous glyph
                }
            } else if (b == ' ' || b == '\t') {
                if (!current.empty()) {
                    words.push_back(std::move(current));
                    current.clear();
                }
            } else {
                Glyph g;
                g.bytes = ch;
                g.span = si; // the span of the glyph's lead byte
                current.push_back(std::move(g));
            }
        }
    }
    if (!current.empty()) {
        words.push_back(std::move(current));
    }
    return words;
}

// The width of the words on one line: their lengths plus one space between neighbours.
std::size_t natural_width(const std::vector<Word>& words) {
    std::size_t width = 0;
    for (const Word& w : words) {
        if (width > 0) {
            width += 1;
        }
        width += w.size();
    }
    return width;
}

// The length of the longest word, or 0 for none.
std::size_t longest_word(const std::vector<Word>& words) {
    std::size_t longest = 0;
    for (const Word& w : words) {
        if (w.size() > longest) {
            longest = w.size();
        }
    }
    return longest;
}

// Wraps the words into lines of at most `width` glyphs, greedily; a long word is cut into pieces
// of `width` and the last piece can still take the next words.
std::vector<Line> wrap_words(const std::vector<Word>& words, std::size_t width) {
    if (width < 1) {
        width = 1;
    }
    std::vector<Line> lines(1);
    for (const Word& word : words) {
        if (!lines.back().empty() && lines.back().size() + 1 + word.size() <= width) {
            Glyph space;
            space.bytes = " ";
            space.span = (lines.back().back().span == word.front().span)
                             ? word.front().span
                             : static_cast<std::size_t>(-1);
            lines.back().push_back(std::move(space));
            lines.back().insert(lines.back().end(), word.begin(), word.end());
        } else {
            if (!lines.back().empty()) {
                lines.emplace_back();
            }
            std::vector<Glyph> rest = word;
            while (rest.size() > width) {
                for (std::size_t i = 0; i < width; ++i) {
                    lines.back().push_back(rest[i]);
                }
                lines.emplace_back();
                rest.erase(rest.begin(), rest.begin() + width);
            }
            lines.back().insert(lines.back().end(), rest.begin(), rest.end());
        }
    }
    return lines;
}

// Hands out `spare` characters, one at a time, to the column furthest below `target`, the
// leftmost on a tie; `spare` is reduced as it is spent and returned for the next call.
void grow_towards(std::vector<std::size_t>& width, const std::vector<std::size_t>& target,
                  std::size_t& spare) {
    while (spare > 0) {
        std::size_t best = 0;
        std::size_t best_deficit = 0;
        bool found = false;
        for (std::size_t c = 0; c < width.size(); ++c) {
            if (target[c] > width[c]) {
                const std::size_t deficit = target[c] - width[c];
                if (!found || deficit > best_deficit) {
                    found = true;
                    best = c;
                    best_deficit = deficit;
                }
            }
        }
        if (!found) {
            return;
        }
        width[best] += 1;
        spare -= 1;
    }
}

} // namespace

std::vector<std::size_t> fit_columns(const std::vector<std::size_t>& natural,
                                     const std::vector<std::size_t>& minimum, std::size_t total) {
    const std::size_t columns = natural.size();
    if (columns == 0) {
        return {};
    }
    std::vector<std::size_t> target(columns);
    std::vector<std::size_t> least(columns);
    for (std::size_t c = 0; c < columns; ++c) {
        target[c] = std::max(natural[c], std::size_t{1});
        const std::size_t min_in = c < minimum.size() ? minimum[c] : 1;
        least[c] = std::min(std::max(min_in, std::size_t{1}), target[c]);
    }
    const std::size_t gaps = 3 * (columns - 1);
    const std::size_t available = total > gaps ? total - gaps : 0;
    std::size_t sum = 0;
    for (const std::size_t t : target) {
        sum += t;
    }
    if (sum <= available) {
        return target;
    }
    std::vector<std::size_t> width(columns, 1);
    if (available > columns) {
        std::size_t spare = available - columns;
        grow_towards(width, least, spare);
        grow_towards(width, target, spare);
    }
    return width;
}

namespace {

// Writes the glyphs of one laid-out line of a cell, recording the styled runs as it goes.
void write_cell_line(const Line& line, const ManualCell& cell, TableLayout& out,
                     std::size_t& chars) {
    std::size_t i = 0;
    while (i < line.size()) {
        const std::size_t span = line[i].span;
        std::size_t begin = chars;
        std::size_t j = i;
        while (j < line.size() && line[j].span == span) {
            out.text += line[j].bytes;
            ++chars;
            ++j;
        }
        const std::size_t end = chars;
        if (span != static_cast<std::size_t>(-1) && span < cell.size()) {
            const ManualSpan& s = cell[span];
            if (s.kind == SpanKind::Bold) {
                out.bold.push_back({begin, end});
            } else if (s.kind == SpanKind::Code) {
                out.code.push_back({begin, end});
            } else if (s.kind == SpanKind::Link) {
                out.links.push_back({begin, end, s.target});
            }
        }
        i = j;
    }
}

// Pads the laid-out line of a cell on the right to its column's width, writing styled runs.
void write_padded_cell(const std::vector<Line>& cell_lines, std::size_t line_index,
                       std::size_t width, const ManualCell& cell, TableLayout& out,
                       std::size_t& chars) {
    if (line_index < cell_lines.size()) {
        write_cell_line(cell_lines[line_index], cell, out, chars);
    }
    const std::size_t written = line_index < cell_lines.size() ? cell_lines[line_index].size() : 0;
    for (std::size_t p = written; p < width; ++p) {
        out.text += ' ';
        ++chars;
    }
}

// The laid-out lines of every cell in a row, one entry per column.
std::vector<std::vector<Line>> wrap_row(const ManualRow& row, const std::vector<std::size_t>& width,
                                        std::size_t columns) {
    std::vector<std::vector<Line>> wrapped(columns);
    for (std::size_t c = 0; c < columns; ++c) {
        const ManualCell& cell = c < row.size() ? row[c] : ManualCell{};
        wrapped[c] = wrap_words(cell_words(cell), width[c]);
    }
    return wrapped;
}

} // namespace

TableLayout layout_table(const ManualBlock& table, std::size_t total) {
    std::size_t columns = table.columns;
    for (const ManualRow& row : table.rows) {
        if (row.size() > columns) {
            columns = row.size();
        }
    }
    std::vector<std::size_t> natural(columns, 1);
    std::vector<std::size_t> minimum(columns, 1);
    for (std::size_t c = 0; c < columns; ++c) {
        for (const ManualRow& row : table.rows) {
            if (c < row.size()) {
                const std::vector<Word> words = cell_words(row[c]);
                natural[c] = std::max(natural[c], natural_width(words));
                minimum[c] = std::max(minimum[c], longest_word(words));
            }
        }
    }
    const std::vector<std::size_t> width = fit_columns(natural, minimum, total);

    TableLayout out;
    std::size_t chars = 0;
    for (std::size_t r = 0; r < table.rows.size(); ++r) {
        if (r > 0) {
            out.text += '\n';
            ++chars;
        }
        const std::vector<std::vector<Line>> wrapped = wrap_row(table.rows[r], width, columns);
        std::size_t max_lines = 1;
        for (const std::vector<Line>& cell_lines : wrapped) {
            if (cell_lines.size() > max_lines) {
                max_lines = cell_lines.size();
            }
        }
        for (std::size_t li = 0; li < max_lines; ++li) {
            if (li > 0) {
                out.text += '\n';
                ++chars;
            }
            for (std::size_t c = 0; c < columns; ++c) {
                if (c > 0) {
                    out.text += " | ";
                    chars += 3;
                }
                write_padded_cell(wrapped[c], li, width[c],
                                  c < table.rows[r].size() ? table.rows[r][c] : ManualCell{}, out,
                                  chars);
            }
        }
        if (r == 0) {
            out.header_end = chars;
            out.text += '\n';
            ++chars;
            for (std::size_t c = 0; c < columns; ++c) {
                if (c > 0) {
                    out.text += "-+-";
                    chars += 3;
                }
                for (std::size_t d = 0; d < width[c]; ++d) {
                    out.text += '-';
                    ++chars;
                }
            }
        }
    }
    out.length = chars;
    return out;
}

} // namespace vermilion
