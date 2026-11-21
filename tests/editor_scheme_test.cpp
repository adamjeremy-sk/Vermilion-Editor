// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the editor style scheme is written from the resources and found by GtkSourceView.

#include <gio/gio.h>
#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>

#include "ui/editor_scheme.h"
#include "vermilion_resources.h"

namespace {

namespace fs = std::filesystem;
using vermilion::ui::editor_scheme;
using vermilion::ui::scheme_folder;
using vermilion::ui::write_scheme_files;

constexpr char kSchemeResource[] =
    "/io/github/adamjeremy_sk/Vermilion/styles/vermilion-classic.xml";
constexpr char kSchemeFile[] = "vermilion-classic.xml";

// The test's own cache folder: XDG_CACHE_HOME points here before GLib is asked for the cache
// folder, so the tests never write into the real cache of the person who runs them.
std::string& cache_root() {
    static std::string root;
    return root;
}

class EditorSchemeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gchar* made = g_dir_make_tmp("vermilion-scheme-XXXXXX", nullptr);
        std::unique_ptr<gchar, decltype(&g_free)> folder(made, &g_free);
        ASSERT_TRUE(static_cast<bool>(folder));
        cache_root() = folder.get();
        g_setenv("XDG_CACHE_HOME", cache_root().c_str(), TRUE);
        gtk_source_init();
        vermilion_register_resource();
    }

    static void TearDownTestSuite() {
        std::error_code error;
        fs::remove_all(cache_root(), error);
    }
};

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The text of the scheme resource, "" when it is missing.
std::string resource_text() {
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_resources_lookup_data(kSchemeResource, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
        &g_bytes_unref);
    if (!bytes) {
        return "";
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    return std::string(data, size);
}

// The background colour that the scheme's style sets, "" when it sets none, and "(no style)" when
// the scheme has no style of that name.
std::string background_of(GtkSourceStyleScheme* scheme, const char* style_id) {
    GtkSourceStyle* style = gtk_source_style_scheme_get_style(scheme, style_id); // borrowed
    if (style == nullptr) {
        return "(no style)";
    }
    gboolean is_set = FALSE;
    gchar* raw = nullptr;
    g_object_get(style, "background-set", &is_set, "background", &raw, nullptr);
    std::unique_ptr<gchar, decltype(&g_free)> color(raw, &g_free);
    if (is_set == FALSE || !color) {
        return "";
    }
    return std::string(color.get());
}

TEST_F(EditorSchemeTest, TheFolderIsUnderTheUserCacheFolder) {
    EXPECT_EQ(scheme_folder(), cache_root() + "/vermilion/styles");
}

TEST_F(EditorSchemeTest, WritesTheSchemeFileFromTheResources) {
    const std::string folder = cache_root() + "/fresh/deeper";
    ASSERT_TRUE(write_scheme_files(folder));
    const std::string text = read_file(folder + "/" + kSchemeFile);
    EXPECT_FALSE(text.empty());
    EXPECT_EQ(text, resource_text());
    EXPECT_NE(text.find("#DCE6F5"), std::string::npos);
}

TEST_F(EditorSchemeTest, AnUpToDateFileIsNotWrittenAgain) {
    const std::string folder = cache_root() + "/current";
    ASSERT_TRUE(write_scheme_files(folder));
    const fs::path file = fs::path(folder) / kSchemeFile;
    const fs::file_time_type now = fs::file_time_type::clock::now();
    std::error_code error;
    fs::last_write_time(file, now - std::chrono::hours(48), error);
    ASSERT_FALSE(error);
    EXPECT_TRUE(write_scheme_files(folder));
    // A rewrite would have made the file's time recent again.
    EXPECT_LT(fs::last_write_time(file, error), now - std::chrono::hours(24));
    EXPECT_FALSE(error);
}

TEST_F(EditorSchemeTest, AStaleFileIsReplaced) {
    const std::string folder = cache_root() + "/stale";
    ASSERT_TRUE(write_scheme_files(folder));
    const std::string file = folder + "/" + kSchemeFile;
    ASSERT_TRUE(g_file_set_contents(file.c_str(), "old text", -1, nullptr));
    EXPECT_EQ(read_file(file), "old text");
    EXPECT_TRUE(write_scheme_files(folder));
    EXPECT_EQ(read_file(file), resource_text());
}

TEST_F(EditorSchemeTest, AFolderThatCannotBeMadeGivesFalse) {
    const std::string blocker = cache_root() + "/blocker";
    ASSERT_TRUE(g_file_set_contents(blocker.c_str(), "x", -1, nullptr));
    EXPECT_FALSE(write_scheme_files(blocker + "/sub"));
}

TEST_F(EditorSchemeTest, FindsTheVermilionScheme) {
    GtkSourceStyleScheme* scheme = editor_scheme(false);
    ASSERT_NE(scheme, nullptr);
    EXPECT_STREQ(gtk_source_style_scheme_get_id(scheme), "vermilion-classic");
}

TEST_F(EditorSchemeTest, TheSchemeFileIsInTheCacheFolder) {
    ASSERT_NE(editor_scheme(false), nullptr);
    EXPECT_EQ(read_file(scheme_folder() + "/" + kSchemeFile), resource_text());
}

TEST_F(EditorSchemeTest, TheCurrentLineIsLightBlue) {
    GtkSourceStyleScheme* scheme = editor_scheme(false);
    ASSERT_NE(scheme, nullptr);
    EXPECT_EQ(background_of(scheme, "current-line"), "#DCE6F5");
}

TEST_F(EditorSchemeTest, TheCurrentLineNumberHasTheSameBackground) {
    GtkSourceStyleScheme* scheme = editor_scheme(false);
    ASSERT_NE(scheme, nullptr);
    EXPECT_EQ(background_of(scheme, "current-line-number"), "#DCE6F5");
}

TEST_F(EditorSchemeTest, EveryOtherStyleComesFromTheClassicParent) {
    GtkSourceStyleScheme* scheme = editor_scheme(false);
    ASSERT_NE(scheme, nullptr);
    EXPECT_NE(gtk_source_style_scheme_get_style(scheme, "text"), nullptr);
    EXPECT_NE(gtk_source_style_scheme_get_style(scheme, "def:comment"), nullptr);
}

TEST_F(EditorSchemeTest, EveryCallGivesTheSameScheme) {
    EXPECT_EQ(editor_scheme(false), editor_scheme(false));
}

} // namespace
