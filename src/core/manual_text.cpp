// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Lays out a parsed manual page as one text with the ranges the Help view styles.

#include "core/manual_text.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/manual_table.h"

namespace vermilion {

namespace {

// U+2022 and a space (two characters), written as escapes so the source stays ASCII.
constexpr char kBullet[] = "\xE2\x80\xA2 ";

// Whether a block leaves any trace in the text: a skipped block leaves no text and no separator.
bool has_content(const ManualBlock& block) {
    switch (block.kind) {
    case ManualBlockKind::Heading:
    case ManualBlockKind::Paragraph:
        for (const ManualSpan& span : block.spans) {
            if (!span.text.empty()) {
                return true;
            }
        }
        return false;
    case ManualBlockKind::ListItem:
        return true;
    case ManualBlockKind::Table: {
        std::size_t columns = block.columns;
        for (const ManualRow& row : block.rows) {
            if (row.size() > columns) {
                columns = row.size();
            }
        }
        return !block.rows.empty() && columns > 0;
    }
    }
    return false;
}

// Writes one page into a ManualText, keeping how many characters it has written so far.
class Builder {
public:
    // Starts writing into `out`, which begins empty.
    explicit Builder(ManualText& out) : out_(out) {}

    // Writes every block that has content, with one empty line between them (or one newline
    // between two list items), and sets the length.
    void add(const ManualPage& page) {
        const ManualBlock* previous = nullptr;
        for (const ManualBlock& block : page.blocks) {
            if (!has_content(block)) {
                continue;
            }
            if (previous != nullptr) {
                const bool items = previous->kind == ManualBlockKind::ListItem &&
                                   block.kind == ManualBlockKind::ListItem;
                text(items ? "\n" : "\n\n");
            }
            switch (block.kind) {
            case ManualBlockKind::Heading:
                heading(block);
                break;
            case ManualBlockKind::Paragraph:
                spans(block.spans);
                break;
            case ManualBlockKind::ListItem:
                list_item(block);
                break;
            case ManualBlockKind::Table:
                table(block);
                break;
            }
            previous = &block;
        }
        out_.length = length_;
    }

private:
    // Appends `s` to the text and counts its characters.
    void text(std::string_view s) {
        out_.text += s;
        length_ += character_count(s);
    }

    // Records the styled span that ends at length_, if it is not empty and is styled.
    void mark(const ManualSpan& span, std::size_t begin) {
        if (length_ <= begin) {
            return;
        }
        switch (span.kind) {
        case SpanKind::Bold:
            out_.bold.push_back({begin, length_});
            break;
        case SpanKind::Code:
            out_.code.push_back({begin, length_});
            break;
        case SpanKind::Link:
            out_.links.push_back({begin, length_, span.target});
            break;
        case SpanKind::Text:
            break;
        }
    }

    // Writes the spans of a block, recording each styled one as it goes.
    void spans(const std::vector<ManualSpan>& list) {
        for (const ManualSpan& span : list) {
            const std::size_t begin = length_;
            text(span.text);
            mark(span, begin);
        }
    }

    // Writes a heading: its words, then its mark (level clamped to 1 to 6, all its words as title).
    void heading(const ManualBlock& block) {
        const std::size_t begin = length_;
        spans(block.spans);
        HeadingMark heading_mark;
        heading_mark.begin = begin;
        heading_mark.end = length_;
        heading_mark.level = std::clamp(block.level, 1, 6);
        for (const ManualSpan& span : block.spans) {
            heading_mark.title += span.text;
        }
        out_.headings.push_back(std::move(heading_mark));
    }

    // Writes a list item: the bullet and a space, then its words; the range covers all of it.
    void list_item(const ManualBlock& block) {
        const std::size_t begin = length_;
        text(kBullet);
        spans(block.spans);
        out_.list_items.push_back({begin, length_});
    }

    // Writes a table: its laid-out text, its range, its header range and its styled spans.
    void table(const ManualBlock& block) {
        const TableLayout layout = layout_table(block, kManualColumns);
        const std::size_t begin = length_;
        text(layout.text);
        out_.tables.push_back({begin, length_});
        out_.table_headers.push_back({begin, begin + layout.header_end});
        for (const TextRange& range : layout.bold) {
            out_.bold.push_back({begin + range.begin, begin + range.end});
        }
        for (const TextRange& range : layout.code) {
            out_.code.push_back({begin + range.begin, begin + range.end});
        }
        for (const LinkRange& link : layout.links) {
            out_.links.push_back({begin + link.begin, begin + link.end, link.target});
        }
    }

    ManualText& out_;        // the result being written
    std::size_t length_ = 0; // the characters written so far
};

} // namespace

std::size_t character_count(std::string_view text) {
    std::size_t count = 0;
    for (const char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

ManualText build_manual_text(const ManualPage& page) {
    ManualText built;
    Builder(built).add(page);
    return built;
}

const LinkRange* link_at(const ManualText& built, std::size_t offset) {
    const auto it = std::upper_bound(
        built.links.begin(), built.links.end(), offset,
        [](std::size_t value, const LinkRange& link) { return value < link.begin; });
    if (it == built.links.begin()) {
        return nullptr;
    }
    const LinkRange* candidate = std::addressof(*std::prev(it));
    return offset < candidate->end ? candidate : nullptr;
}

} // namespace vermilion
