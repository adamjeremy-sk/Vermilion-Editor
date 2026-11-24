// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the result rows for the real manual (docs/manual/*.md) are sound.

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
#include "core/manual_results.h"
#include "core/manual_text.h"
#include "core/search_options.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

using vermilion::SearchOptions;
using vermilion::SearchPage;

// Every *.md file of docs/manual, sorted by name, laid out as page text.
std::vector<SearchPage> read_pages() {
    std::vector<SearchPage> pages;
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
        pages.push_back({name, name,
                         vermilion::build_manual_text(vermilion::parse_manual(text.str()),
                                                      vermilion::kManualColumns)
                             .text});
    }
    std::sort(pages.begin(), pages.end(),
              [](const SearchPage& a, const SearchPage& b) { return a.file < b.file; });
    return pages;
}

std::string lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + 32);
        }
    }
    return text;
}

TEST(ManualResultsFiles, EveryMatchHasASnippetThatHoldsIt) {
    SearchOptions options;
    options.text = "file";
    const std::vector<SearchPage> pages = read_pages();
    const std::vector<vermilion::PageResult> results = vermilion::search_pages(pages, options);
    ASSERT_FALSE(results.empty());
    for (const vermilion::PageResult& result : results) {
        SCOPED_TRACE(result.file);
        EXPECT_GT(result.count, std::size_t{0});
        EXPECT_NE(lower(result.snippet).find("file"), std::string::npos) << result.snippet;
        EXPECT_LE(vermilion::character_count(result.snippet), vermilion::kSnippetColumns + 2);
        EXPECT_EQ(result.snippet.find('\n'), std::string::npos);
    }
}

TEST(ManualResultsFiles, TheSettingsPageIsFoundBySettings) {
    SearchOptions options;
    options.text = "settings";
    options.whole_word = true;
    bool found = false;
    for (const vermilion::PageResult& result : vermilion::search_pages(read_pages(), options)) {
        found = found || result.file == "settings.md";
    }
    EXPECT_TRUE(found);
}

} // namespace
