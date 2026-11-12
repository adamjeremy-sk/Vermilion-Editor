// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that every docs/manual/*.md file, and nothing else, is compiled into the program.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#include "vermilion_manual.h"

#ifndef VERMILION_MANUAL_DIR
#error "VERMILION_MANUAL_DIR must name docs/manual; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;

constexpr const char* kManualPrefix = "/io/github/adamjeremy_sk/Vermilion/manual";

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;
using NamesPtr = std::unique_ptr<char*, decltype(&g_strfreev)>;

void register_manual() {
    static const bool registered = [] {
        vermilion_manual_register_resource();
        return true;
    }();
    (void)registered;
}

// The names of the *.md files in docs/manual and their contents.
std::set<std::string> manual_files() {
    std::set<std::string> names;
    std::error_code error;
    for (fs::directory_iterator it(VERMILION_MANUAL_DIR, error), end; !error && it != end;
         it.increment(error)) {
        if (it->path().extension() == ".md") {
            names.insert(it->path().filename().string());
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

TEST(ManualResources, EveryManualFileIsInTheProgramUnchanged) {
    register_manual();
    std::set<std::string> names = manual_files();
    ASSERT_GE(names.size(), std::size_t{1});
    for (const std::string& name : names) {
        SCOPED_TRACE(name);
        std::string path = std::string(kManualPrefix) + "/" + name;
        BytesPtr bytes(g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                       &g_bytes_unref);
        ASSERT_TRUE(bytes != nullptr);
        gsize size = 0;
        const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
        EXPECT_EQ(std::string(data, size), file_text(name));
    }
}

TEST(ManualResources, NothingElseIsInTheProgram) {
    register_manual();
    NamesPtr children(
        g_resources_enumerate_children(kManualPrefix, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
        &g_strfreev);
    ASSERT_TRUE(children != nullptr);
    std::set<std::string> in_program;
    for (char** name = children.get(); *name != nullptr; ++name) {
        in_program.insert(*name);
    }
    EXPECT_TRUE(in_program == manual_files());
}

} // namespace
