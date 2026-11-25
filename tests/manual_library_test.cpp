// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for ManualLibrary: the manual made from text, read from the program's resources, and kept
// by a library that is prepared on a worker thread (no window and no display).

#include <glib.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_text.h"
#include "ui/manual_library.h"
#include "vermilion_manual.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

using vermilion::OutlineTopic;
using vermilion::SearchPage;
using vermilion::ui::ManualData;
using vermilion::ui::ManualLibrary;

namespace fs = std::filesystem;

void register_manual() {
    static const bool registered = [] {
        vermilion_manual_register_resource();
        return true;
    }();
    (void)registered;
}

// Runs the main loop until `done` is true, for at most ten seconds.
void pump_until(const std::function<bool()>& done) {
    const gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// Runs the main loop for `milliseconds`.
void pump_for(int milliseconds) {
    const gint64 deadline = g_get_monotonic_time() + milliseconds * 1000;
    while (g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

std::vector<std::string> files_of(const std::vector<OutlineTopic>& outline) {
    std::vector<std::string> files;
    for (const OutlineTopic& topic : outline) {
        files.push_back(topic.file);
    }
    return files;
}

std::set<std::string> manual_files() {
    std::set<std::string> names;
    for (const auto& entry : fs::directory_iterator(VERMILION_MANUAL_DIR)) {
        if (entry.path().extension() == ".md") {
            names.insert(entry.path().filename().string());
        }
    }
    return names;
}

std::string file_text(const std::string& name) {
    std::ifstream in(fs::path(VERMILION_MANUAL_DIR) / name, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// A small manual: "index.md" links to b.md and then a.md; c.md is not linked.
std::map<std::string, std::string> small_manual() {
    return {{"index.md", "# Home\n\nSee [B](b.md) and then [A](a.md).\n"},
            {"a.md", "# Alpha\n\nText.\n\n## First\n\nMore.\n"},
            {"b.md", "# Beta\n\n| x | y |\n|---|---|\n| one | two |\n"},
            {"c.md", "# Gamma\n"}};
}

TEST(ManualLibraryTest, TheTreeStartsWithTheIndexAndFollowsItsLinks) {
    const ManualData data = vermilion::ui::make_manual_data(small_manual());
    EXPECT_EQ(files_of(data.outline), (std::vector<std::string>{"index.md", "b.md", "a.md"}));
    EXPECT_EQ(data.outline[0].title, "Home");
    EXPECT_EQ(data.outline[2].title, "Alpha");
    ASSERT_EQ(data.outline[2].headings.size(), 1u);
    EXPECT_EQ(data.outline[2].headings[0].title, "First");
}

TEST(ManualLibraryTest, EveryTopicIsParsedAlsoOneThatTheTreeLeavesOut) {
    const ManualData data = vermilion::ui::make_manual_data(small_manual());
    ASSERT_EQ(data.pages.size(), 4u);
    EXPECT_EQ(data.pages.at("c.md").blocks.size(), 1u);
    EXPECT_EQ(data.pages.at("a.md").blocks.size(), 4u);
    EXPECT_EQ(data.pages.at("b.md").blocks.back().kind, vermilion::ManualBlockKind::Table);
}

TEST(ManualLibraryTest, WithoutAnIndexTheTreeIsEmptyAndThePagesAreKept) {
    std::map<std::string, std::string> texts = small_manual();
    texts.erase("index.md");
    const ManualData data = vermilion::ui::make_manual_data(texts);
    EXPECT_TRUE(data.outline.empty());
    EXPECT_EQ(data.pages.size(), 3u);
    EXPECT_TRUE(vermilion::ui::make_manual_data({}).pages.empty());
}

TEST(ManualLibraryTest, TheSearchPagesFollowTheTreeAndAreLaidOutForTheColumns) {
    const ManualData data = vermilion::ui::make_manual_data(small_manual());
    const std::vector<SearchPage> wide = vermilion::ui::manual_search_pages(data, 100);
    ASSERT_EQ(wide.size(), 3u);
    EXPECT_EQ(wide[0].file, "index.md");
    EXPECT_EQ(wide[1].file, "b.md");
    EXPECT_EQ(wide[1].title, "Beta");
    EXPECT_EQ(wide[2].file, "a.md");
    EXPECT_EQ(wide[1].text, vermilion::build_manual_text(data.pages.at("b.md"), 100).text);
    const std::vector<SearchPage> narrow = vermilion::ui::manual_search_pages(data, 4);
    ASSERT_EQ(narrow.size(), 3u);
    EXPECT_NE(narrow[1].text, wide[1].text); // the table depends on the width
    EXPECT_EQ(narrow[2].text, wide[2].text); // a page without a table does not
}

TEST(ManualLibraryTest, EveryTopicOfTheProgramIsRead) {
    register_manual();
    const ManualData data = vermilion::ui::load_manual();
    std::set<std::string> read;
    for (const auto& entry : data.pages) {
        read.insert(entry.first);
    }
    EXPECT_EQ(read, manual_files());
    EXPECT_GE(read.size(), 10u);
}

TEST(ManualLibraryTest, ThePagesAreTheParsedFiles) {
    register_manual();
    const ManualData data = vermilion::ui::load_manual();
    for (const std::string& name : manual_files()) {
        SCOPED_TRACE(name);
        const vermilion::ManualPage expected = vermilion::parse_manual(file_text(name));
        ASSERT_EQ(data.pages.count(name), 1u);
        EXPECT_EQ(vermilion::build_manual_text(data.pages.at(name), 100).text,
                  vermilion::build_manual_text(expected, 100).text);
    }
}

TEST(ManualLibraryTest, TheTreeOfTheProgramsManualStartsWithTheIndex) {
    register_manual();
    const ManualData data = vermilion::ui::load_manual();
    ASSERT_FALSE(data.outline.empty());
    EXPECT_EQ(data.outline[0].file, "index.md");
    std::set<std::string> in_tree;
    for (const OutlineTopic& topic : data.outline) {
        EXPECT_TRUE(in_tree.insert(topic.file).second) << topic.file << " is in the tree twice";
        EXPECT_FALSE(topic.title.empty());
        EXPECT_EQ(data.pages.count(topic.file), 1u);
    }
    EXPECT_GE(in_tree.size(), 10u);
}

TEST(ManualLibraryTest, TopicTextGivesTheFileAndRefusesWhatIsNotATopic) {
    register_manual();
    const std::optional<std::string> index = vermilion::ui::topic_text("index.md");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(*index, file_text("index.md"));
    for (const char* name : {"", "../index.md", "index", "a b.md", "no-such-topic.md"}) {
        SCOPED_TRACE(name);
        EXPECT_FALSE(vermilion::ui::topic_text(name).has_value());
    }
}

TEST(ManualLibraryTest, ALibraryIsNotReadyUntilItIsPrepared) {
    register_manual();
    ManualLibrary library;
    EXPECT_FALSE(library.ready());
    EXPECT_EQ(library.data(), nullptr);
    pump_for(50);
    EXPECT_FALSE(library.ready());
    EXPECT_EQ(library.data(), nullptr);
}

TEST(ManualLibraryTest, PreparingGivesTheManualOnTheMainLoopAndNotBefore) {
    register_manual();
    ManualLibrary library;
    library.prepare();
    EXPECT_FALSE(library.ready()); // the hand-back is always later, on the main loop
    pump_until([&library] { return library.ready(); });
    ASSERT_TRUE(library.ready());
    ASSERT_NE(library.data(), nullptr);
    const ManualData expected = vermilion::ui::load_manual();
    EXPECT_EQ(files_of(library.data()->outline), files_of(expected.outline));
    EXPECT_EQ(library.data()->pages.size(), expected.pages.size());
}

TEST(ManualLibraryTest, PreparingAgainChangesNothing) {
    register_manual();
    ManualLibrary library;
    library.prepare();
    library.prepare();
    pump_until([&library] { return library.ready(); });
    ASSERT_TRUE(library.ready());
    const ManualData* first = library.data();
    int told = 0;
    ManualLibrary::Wait wait = library.on_ready([&told] { ++told; }); // never called: it is ready
    library.prepare();
    library.prepare_when_idle();
    pump_for(50);
    EXPECT_EQ(library.data(), first);
    EXPECT_EQ(told, 0); // a second read would have told the waiter
}

TEST(ManualLibraryTest, PreparingWhenIdleWaitsForTheMainLoop) {
    register_manual();
    ManualLibrary library;
    library.prepare_when_idle();
    library.prepare_when_idle(); // a second request is the same request
    EXPECT_FALSE(library.ready());
    pump_until([&library] { return library.ready(); });
    EXPECT_TRUE(library.ready());
}

TEST(ManualLibraryTest, EachWaiterIsToldOnceWhenTheManualIsReady) {
    register_manual();
    ManualLibrary library;
    int first = 0;
    int second = 0;
    bool ready_when_told = false;
    ManualLibrary::Wait one = library.on_ready([&] {
        ++first;
        ready_when_told = library.ready();
    });
    ManualLibrary::Wait two = library.on_ready([&second] { ++second; });
    library.prepare();
    pump_until([&] { return first > 0 && second > 0; });
    pump_for(50);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    EXPECT_TRUE(ready_when_told);
}

TEST(ManualLibraryTest, AWaitThatIsGivenUpIsNotTold) {
    register_manual();
    ManualLibrary library;
    int kept = 0;
    int given_up = 0;
    int replaced = 0;
    ManualLibrary::Wait keeper = library.on_ready([&kept] { ++kept; });
    {
        ManualLibrary::Wait gone = library.on_ready([&given_up] { ++given_up; });
    }
    ManualLibrary::Wait moved = library.on_ready([&replaced] { ++replaced; });
    ManualLibrary::Wait other = std::move(moved);  // the claim goes with the move
    other = library.on_ready([&kept] { ++kept; }); // the first claim of `other` ends here
    library.prepare();
    pump_until([&library] { return library.ready(); });
    pump_for(50);
    EXPECT_EQ(kept, 2);
    EXPECT_EQ(given_up, 0);
    EXPECT_EQ(replaced, 0);
}

TEST(ManualLibraryTest, ACallbackMayGiveUpAnotherWait) {
    register_manual();
    ManualLibrary library;
    int second = 0;
    ManualLibrary::Wait later;
    ManualLibrary::Wait first = library.on_ready([&later] { later = ManualLibrary::Wait(); });
    later = library.on_ready([&second] { ++second; });
    library.prepare();
    pump_until([&library] { return library.ready(); });
    pump_for(50);
    EXPECT_EQ(second, 0);
}

TEST(ManualLibraryTest, ADestroyedLibraryTellsNobodyAndAWaitMayOutliveIt) {
    register_manual();
    int told = 0;
    ManualLibrary::Wait wait;
    {
        ManualLibrary library;
        wait = library.on_ready([&told] { ++told; });
        library.prepare();
    }
    pump_for(200);
    EXPECT_EQ(told, 0);
    wait = ManualLibrary::Wait(); // the library is gone; ending the claim is harmless
    SUCCEED();
}

TEST(ManualLibraryTest, ADestroyedLibraryWithARequestForLaterNeverStarts) {
    register_manual();
    int told = 0;
    ManualLibrary::Wait wait;
    {
        ManualLibrary library;
        wait = library.on_ready([&told] { ++told; });
        library.prepare_when_idle();
    }
    pump_for(200);
    EXPECT_EQ(told, 0);
}

} // namespace
