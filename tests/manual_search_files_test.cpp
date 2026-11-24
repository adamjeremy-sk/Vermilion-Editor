// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that searching the real manual (docs/manual/*.md) gives sound matches.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_search.h"
#include "core/manual_text.h"
#include "core/search_options.h"
#include "manual_test_columns.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

using vermilion::SearchOptions;

struct Page {
    std::string name;
    std::string text;
};

// Every *.md file of docs/manual, sorted by name, laid out as page text.
std::vector<Page> read_pages() {
    std::vector<Page> pages;
    std::error_code error;
    for (fs::directory_iterator it(VERMILION_MANUAL_DIR, error), end; !error && it != end;
         it.increment(error)) {
        if (it->path().extension() != ".md") {
            continue;
        }
        std::ifstream in(it->path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        const std::string name = it->path().filename().string();
        pages.push_back({name, vermilion::build_manual_text(vermilion::parse_manual(text.str()),
                                                            kManualTestColumns)
                                   .text});
    }
    std::sort(pages.begin(), pages.end(),
              [](const Page& a, const Page& b) { return a.name < b.name; });
    return pages;
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

std::string lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + 32);
        }
    }
    return text;
}

// How many times the lower-case `needle` occurs in the lower-case `text`, without overlap.
std::size_t naive_count(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t at = lower(text).find(needle); at != std::string::npos;
         at = lower(text).find(needle, at + needle.size())) {
        ++count;
    }
    return count;
}

TEST(ManualSearchFiles, EveryMatchHoldsTheSearchedWordsAndTheCountIsRight) {
    for (const std::string needle : {"the", "e", "file"}) {
        SearchOptions options;
        options.text = needle;
        for (const Page& page : read_pages()) {
            SCOPED_TRACE(page.name + " / " + needle);
            const std::vector<vermilion::SearchMatch> found =
                vermilion::find_matches(page.text, options);
            EXPECT_EQ(found.size(), naive_count(page.text, needle));
            std::size_t previous_end = 0;
            for (const vermilion::SearchMatch& match : found) {
                EXPECT_GE(match.begin, previous_end);
                EXPECT_EQ(lower(slice(page.text, match.begin, match.end)), needle);
                previous_end = match.end;
            }
        }
    }
}

} // namespace
