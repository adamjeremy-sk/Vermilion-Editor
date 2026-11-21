// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the dark editor scheme and of the dark palette sheet: files, colours, the parent.

#include <gio/gio.h>
#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "ui/editor_scheme.h"
#include "vermilion_resources.h"

namespace {

namespace fs = std::filesystem;
using vermilion::ui::editor_scheme;
using vermilion::ui::scheme_folder;
using vermilion::ui::write_scheme_files;

constexpr char kPrefix[] = "/io/github/adamjeremy_sk/Vermilion/";
constexpr char kLightFile[] = "vermilion-classic.xml";
constexpr char kDarkFile[] = "vermilion-classic-dark.xml";

// The test's own cache folder: XDG_CACHE_HOME points here before GLib is asked for the cache
// folder, so the tests never write into the real cache of the person who runs them.
std::string& cache_root() {
    static std::string root;
    return root;
}

class EditorSchemeDarkTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gchar* made = g_dir_make_tmp("vermilion-dark-scheme-XXXXXX", nullptr);
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

// The text of the resource below the program's prefix, "" when it is missing.
std::string resource_text(const std::string& name) {
    const std::string path = std::string(kPrefix) + name;
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
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

bool is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || c == '_';
}

// The names that `sheet` defines with "@define-color NAME", sorted.
std::vector<std::string> defined_names(const std::string& sheet) {
    static const std::string kKey = "@define-color ";
    std::vector<std::string> names;
    for (std::size_t at = sheet.find(kKey); at != std::string::npos;
         at = sheet.find(kKey, at + kKey.size())) {
        std::size_t begin = at + kKey.size();
        std::size_t end = begin;
        while (end < sheet.size() && is_name_char(sheet[end])) {
            ++end;
        }
        names.push_back(sheet.substr(begin, end - begin));
    }
    std::sort(names.begin(), names.end());
    return names;
}

// The colour names that `sheet` uses as "@vm_...", each once, sorted.
std::vector<std::string> used_names(const std::string& sheet) {
    static const std::string kKey = "@vm_";
    std::vector<std::string> names;
    for (std::size_t at = sheet.find(kKey); at != std::string::npos;
         at = sheet.find(kKey, at + kKey.size())) {
        std::size_t begin = at + 1;
        std::size_t end = begin;
        while (end < sheet.size() && is_name_char(sheet[end])) {
            ++end;
        }
        names.push_back(sheet.substr(begin, end - begin));
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

TEST_F(EditorSchemeDarkTest, BothSchemeFilesAreWrittenFromTheResources) {
    const std::string folder = cache_root() + "/both";
    ASSERT_TRUE(write_scheme_files(folder));
    EXPECT_EQ(read_file(folder + "/" + kLightFile),
              resource_text(std::string("styles/") + kLightFile));
    const std::string dark = read_file(folder + "/" + kDarkFile);
    EXPECT_FALSE(dark.empty());
    EXPECT_EQ(dark, resource_text(std::string("styles/") + kDarkFile));
    EXPECT_NE(dark.find("#2C3A4F"), std::string::npos);
}

TEST_F(EditorSchemeDarkTest, AMissingDarkFileIsWrittenAndTheLightOneIsLeftAlone) {
    const std::string folder = cache_root() + "/only_dark_missing";
    ASSERT_TRUE(write_scheme_files(folder));
    const fs::path light = fs::path(folder) / kLightFile;
    const fs::path dark = fs::path(folder) / kDarkFile;
    std::error_code error;
    const fs::file_time_type now = fs::file_time_type::clock::now();
    fs::last_write_time(light, now - std::chrono::hours(48), error);
    ASSERT_FALSE(error);
    ASSERT_TRUE(fs::remove(dark, error));
    EXPECT_TRUE(write_scheme_files(folder));
    EXPECT_FALSE(read_file(dark.string()).empty());
    EXPECT_LT(fs::last_write_time(light, error), now - std::chrono::hours(24));
}

TEST_F(EditorSchemeDarkTest, AStaleDarkFileIsReplaced) {
    const std::string folder = cache_root() + "/stale_dark";
    ASSERT_TRUE(write_scheme_files(folder));
    const std::string file = folder + "/" + kDarkFile;
    ASSERT_TRUE(g_file_set_contents(file.c_str(), "old text", -1, nullptr));
    EXPECT_TRUE(write_scheme_files(folder));
    EXPECT_EQ(read_file(file), resource_text(std::string("styles/") + kDarkFile));
}

TEST_F(EditorSchemeDarkTest, AFolderThatCannotBeMadeGivesFalse) {
    const std::string blocker = cache_root() + "/blocker";
    ASSERT_TRUE(g_file_set_contents(blocker.c_str(), "x", -1, nullptr));
    EXPECT_FALSE(write_scheme_files(blocker + "/sub"));
}

TEST_F(EditorSchemeDarkTest, FindsTheDarkScheme) {
    GtkSourceStyleScheme* scheme = editor_scheme(true);
    ASSERT_NE(scheme, nullptr);
    EXPECT_STREQ(gtk_source_style_scheme_get_id(scheme), "vermilion-classic-dark");
    EXPECT_EQ(read_file(scheme_folder() + "/" + kDarkFile),
              resource_text(std::string("styles/") + kDarkFile));
}

TEST_F(EditorSchemeDarkTest, LightAndDarkAreTwoSchemes) {
    GtkSourceStyleScheme* light = editor_scheme(false);
    GtkSourceStyleScheme* dark = editor_scheme(true);
    ASSERT_NE(light, nullptr);
    ASSERT_NE(dark, nullptr);
    EXPECT_NE(light, dark);
    EXPECT_STREQ(gtk_source_style_scheme_get_id(light), "vermilion-classic");
    EXPECT_EQ(editor_scheme(true), dark);
    EXPECT_EQ(editor_scheme(false), light);
}

TEST_F(EditorSchemeDarkTest, TheCurrentLineAndItsNumberAreDarkBlue) {
    GtkSourceStyleScheme* scheme = editor_scheme(true);
    ASSERT_NE(scheme, nullptr);
    EXPECT_EQ(background_of(scheme, "current-line"), "#2C3A4F");
    EXPECT_EQ(background_of(scheme, "current-line-number"), "#2C3A4F");
}

TEST_F(EditorSchemeDarkTest, TheClassicDarkParentExistsAndSuppliesTheOtherStyles) {
    GtkSourceStyleSchemeManager* manager = gtk_source_style_scheme_manager_get_default();
    ASSERT_NE(editor_scheme(true), nullptr);
    EXPECT_NE(gtk_source_style_scheme_manager_get_scheme(manager, "classic-dark"), nullptr);
    GtkSourceStyleScheme* scheme = editor_scheme(true);
    EXPECT_NE(gtk_source_style_scheme_get_style(scheme, "text"), nullptr);
    EXPECT_NE(gtk_source_style_scheme_get_style(scheme, "def:comment"), nullptr);
}

TEST_F(EditorSchemeDarkTest, TheDarkPaletteDefinesTheSameThirteenNamesAsTheLightOne) {
    const std::vector<std::string> light = defined_names(resource_text("classic_light.css"));
    const std::vector<std::string> dark = defined_names(resource_text("classic_dark.css"));
    EXPECT_EQ(light.size(), 13u);
    EXPECT_EQ(dark, light);
}

TEST_F(EditorSchemeDarkTest, EveryColourNameTheStructureUsesIsDefinedByTheDarkPalette) {
    const std::vector<std::string> used = used_names(resource_text("classic_structure.css"));
    const std::vector<std::string> dark = defined_names(resource_text("classic_dark.css"));
    EXPECT_FALSE(used.empty());
    for (const std::string& name : used) {
        EXPECT_TRUE(std::binary_search(dark.begin(), dark.end(), name)) << name;
    }
}

} // namespace
