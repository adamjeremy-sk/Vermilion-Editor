// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the manual's Markdown subset at block level: headings, paragraphs, lists, tables.

#include "core/manual_page.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/manual_spans.h"

namespace vermilion {

namespace {

bool is_blank_char(char c) {
    return c == ' ' || c == '\t';
}

std::string_view trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && is_blank_char(text[begin])) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && is_blank_char(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::size_t indent_of(std::string_view line) {
    std::size_t count = 0;
    while (count < line.size() && line[count] == ' ') {
        ++count;
    }
    return count;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_blank(std::string_view line) {
    return trim(line).empty();
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        std::size_t stop = (end == std::string_view::npos) ? text.size() : end;
        std::string_view line = text.substr(start, stop - start);
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }
        lines.push_back(line);
        start = stop + 1;
    }
    return lines;
}

std::size_t heading_level(std::string_view line) {
    std::size_t count = 0;
    while (count < line.size() && line[count] == '#') {
        ++count;
    }
    if (count == 0 || count > 6) {
        return 0;
    }
    if (count == line.size()) {
        return 0;
    }
    if (!is_blank_char(line[count])) {
        return 0;
    }
    if (trim(line.substr(count)).empty()) {
        return 0;
    }
    return count;
}

bool is_list_item(std::string_view line) {
    std::size_t end = line.size();
    while (end > 0 && is_blank_char(line[end - 1])) {
        --end;
    }
    std::string_view text = line.substr(0, end);
    return text.size() >= 3 && text[0] == '-' && text[1] == ' ';
}

bool starts_with_pipe(std::string_view line) {
    std::string_view text = trim(line);
    return !text.empty() && text[0] == '|';
}

std::vector<std::string_view> split_row(std::string_view line) {
    std::string_view text = trim(line);
    if (!text.empty() && text.front() == '|') {
        text = text.substr(1);
    }
    if (!text.empty() && text.back() == '|') {
        text = text.substr(0, text.size() - 1);
    }
    std::vector<std::string_view> cells;
    std::size_t start = 0;
    while (true) {
        std::size_t pipe = text.find('|', start);
        std::size_t stop = (pipe == std::string_view::npos) ? text.size() : pipe;
        cells.push_back(trim(text.substr(start, stop - start)));
        if (pipe == std::string_view::npos) {
            break;
        }
        start = pipe + 1;
    }
    return cells;
}

bool is_separator_cell(std::string_view cell) {
    if (!cell.empty() && cell.front() == ':') {
        cell = cell.substr(1);
    }
    if (!cell.empty() && cell.back() == ':') {
        cell = cell.substr(0, cell.size() - 1);
    }
    if (cell.empty()) {
        return false;
    }
    for (char c : cell) {
        if (c != '-') {
            return false;
        }
    }
    return true;
}

bool is_separator_row(std::string_view line, std::size_t cells) {
    if (!starts_with_pipe(line)) {
        return false;
    }
    std::vector<std::string_view> parts = split_row(line);
    if (parts.size() != cells) {
        return false;
    }
    for (const std::string_view& part : parts) {
        if (!is_separator_cell(part)) {
            return false;
        }
    }
    return true;
}

std::size_t line_fallback(std::string_view line) {
    std::string_view text = trim(line);
    if (text.empty()) {
        return 0;
    }
    if (text[0] == '>') {
        return 1;
    }
    if (text.substr(0, 3) == "```" || text.substr(0, 3) == "~~~" || text.substr(0, 2) == "+ ") {
        return 1;
    }
    if (text.size() >= 3) {
        bool only_dashes = true;
        for (char c : text) {
            if (c != '-') {
                only_dashes = false;
                break;
            }
        }
        if (only_dashes) {
            return 1;
        }
    }
    std::size_t digits = 0;
    while (digits < text.size() && is_digit(text[digits])) {
        ++digits;
    }
    if (digits > 0 && digits < text.size() && (text[digits] == '.' || text[digits] == ')')) {
        std::size_t after = digits + 1;
        if (after == text.size() || text[after] == ' ') {
            return 1;
        }
    }
    return 0;
}

class PageBuilder {
public:
    explicit PageBuilder(std::vector<std::string_view> lines) : lines_(std::move(lines)) {}

    ManualPage run() {
        while (i_ < lines_.size()) {
            block();
        }
        return std::move(page_);
    }

private:
    void block() {
        std::string_view line = lines_[i_];
        if (is_blank(line)) {
            ++i_;
            return;
        }
        if (heading_level(line) > 0) {
            heading(line);
            return;
        }
        if (is_list_item(line)) {
            list_item();
            return;
        }
        if (i_ + 1 < lines_.size() && starts_with_pipe(line) &&
            is_separator_row(lines_[i_ + 1], split_row(line).size())) {
            table();
            return;
        }
        paragraph();
    }

    void heading(std::string_view line) {
        std::size_t level = heading_level(line);
        ManualBlock block;
        block.kind = ManualBlockKind::Heading;
        block.level = static_cast<int>(level);
        block.spans = parse_spans(trim(line.substr(level)), page_.fallbacks);
        page_.blocks.push_back(std::move(block));
        ++i_;
    }

    void list_item() {
        std::string text = std::string(trim(lines_[i_].substr(1)));
        ++i_;
        while (i_ < lines_.size() && !is_blank(lines_[i_]) && indent_of(lines_[i_]) >= 2) {
            std::string_view part = trim(lines_[i_]);
            if (part.substr(0, 2) == "- ") {
                ++page_.fallbacks;
            }
            text += ' ';
            text += std::string(part);
            ++i_;
        }
        ManualBlock block;
        block.kind = ManualBlockKind::ListItem;
        block.spans = parse_spans(text, page_.fallbacks);
        page_.blocks.push_back(std::move(block));
    }

    void table() {
        ManualBlock block;
        block.kind = ManualBlockKind::Table;
        std::size_t header_cells = split_row(lines_[i_]).size();
        add_row(block, lines_[i_]);
        i_ += 2;
        while (i_ < lines_.size() && starts_with_pipe(lines_[i_])) {
            if (split_row(lines_[i_]).size() != header_cells) {
                ++page_.fallbacks;
            }
            add_row(block, lines_[i_]);
            ++i_;
        }
        page_.blocks.push_back(std::move(block));
    }

    void add_row(ManualBlock& block, std::string_view line) {
        ManualRow row;
        for (const std::string_view& cell : split_row(line)) {
            row.push_back(parse_spans(cell, page_.fallbacks));
        }
        if (row.size() > block.columns) {
            block.columns = row.size();
        }
        block.rows.push_back(std::move(row));
    }

    bool ends_paragraph(std::string_view line) const {
        return is_blank(line) || heading_level(line) > 0 || is_list_item(line) ||
               starts_with_pipe(line);
    }

    void paragraph() {
        if (starts_with_pipe(lines_[i_])) {
            ++page_.fallbacks;
        }
        std::string text;
        while (i_ < lines_.size() && !is_blank(lines_[i_])) {
            std::string_view line = lines_[i_];
            if (!text.empty() && ends_paragraph(line)) {
                break;
            }
            page_.fallbacks += line_fallback(line);
            if (!text.empty()) {
                text += ' ';
            }
            text += std::string(trim(line));
            ++i_;
        }
        ManualBlock block;
        block.kind = ManualBlockKind::Paragraph;
        block.spans = parse_spans(text, page_.fallbacks);
        page_.blocks.push_back(std::move(block));
    }

    std::vector<std::string_view> lines_;
    std::size_t i_ = 0;
    ManualPage page_;
};

} // namespace

ManualPage parse_manual(std::string_view markdown) {
    return PageBuilder(split_lines(markdown)).run();
}

} // namespace vermilion
