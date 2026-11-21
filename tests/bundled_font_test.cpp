// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the bundled font: the files in the resources, the cache folder, the registration with
// fontconfig and Pango. No window and no display.

#include <gtest/gtest.h>

#include <gio/gio.h>
#include <glib.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "ui/bundled_font.h"
#include "vermilion_resources.h"

#ifndef VERMILION_FONT_DIR
#error "VERMILION_FONT_DIR must name third_party/fantasque-sans; tests/CMakeLists.txt defines it"
#endif

namespace {

namespace fs = std::filesystem;
using vermilion::ui::bundled_font_files;
using vermilion::ui::bundled_font_folder;
using vermilion::ui::install_bundled_font;
using vermilion::ui::kBundledFontFamily;
using vermilion::ui::kBundledFontResourceDir;
using vermilion::ui::register_font_files;
using vermilion::ui::write_bundled_font;

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;
using ObjectPtr = std::unique_ptr<GObject, decltype(&g_object_unref)>;

// The test's own cache folder: XDG_CACHE_HOME points here before GLib is asked for the cache
// folder, so the tests never write into the real cache of the person who runs them. The one
// registration with fontconfig of this program is install_bundled_font(), called by
// registered_count(); the other tests do not register the same files again.
std::string& cache_root() {
    static std::string root;
    return root;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The bytes of a file in the resources; empty and `found` false when it is not there.
std::string resource_bytes(const std::string& name, bool& found) {
    const std::string path = std::string(kBundledFontResourceDir) + "/" + name;
    BytesPtr bytes(g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                   &g_bytes_unref);
    found = bytes != nullptr;
    if (!found) {
        return std::string();
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    return std::string(data, size);
}

class BundledFontTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        g_autofree gchar* made = g_dir_make_tmp("vermilion-font-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        cache_root() = made;
        g_setenv("XDG_CACHE_HOME", cache_root().c_str(), TRUE);
        vermilion_register_resource();
    }

    static void TearDownTestSuite() {
        std::error_code error;
        fs::remove_all(cache_root(), error);
    }

    // Installs the font once and returns how many files were registered. The default font map is
    // used before the install (its families are listed), as a window would have done, so the
    // family only appears if the install tells Pango that the configuration changed.
    static int registered_count() {
        static const int count = [] {
            g_autofree PangoFontFamily** families = nullptr;
            int n = 0;
            pango_font_map_list_families(pango_cairo_font_map_get_default(), &families, &n);
            return install_bundled_font();
        }();
        return count;
    }

    // The family of the default font map that carries the bundled font's name; null when absent.
    static PangoFontFamily* find_family(PangoFontFamily*** all, int* n) {
        PangoFontMap* map = pango_cairo_font_map_get_default();
        pango_font_map_list_families(map, all, n);
        for (int i = 0; i < *n; ++i) {
            if (std::string((*all)[i] != nullptr ? pango_font_family_get_name((*all)[i]) : "") ==
                kBundledFontFamily) {
                return (*all)[i];
            }
        }
        return nullptr;
    }

    // The digit width in Pango units of the bundled family at 11 pt, and the family name of the
    // font that was really loaded (a fallback font would show here).
    static int digit_width(PangoWeight weight, PangoStyle style, std::string& loaded_family) {
        PangoFontMap* map = pango_cairo_font_map_get_default();
        ObjectPtr context(G_OBJECT(pango_font_map_create_context(map)), &g_object_unref);
        PangoFontDescription* description = pango_font_description_new();
        pango_font_description_set_family(description, kBundledFontFamily);
        pango_font_description_set_size(description, 11 * PANGO_SCALE);
        pango_font_description_set_weight(description, weight);
        pango_font_description_set_style(description, style);
        ObjectPtr font(
            G_OBJECT(pango_font_map_load_font(map, PANGO_CONTEXT(context.get()), description)),
            &g_object_unref);
        pango_font_description_free(description);
        if (!font) {
            return -1;
        }
        PangoFontDescription* described = pango_font_describe(PANGO_FONT(font.get()));
        loaded_family = pango_font_description_get_family(described);
        pango_font_description_free(described);
        PangoFontMetrics* metrics = pango_font_get_metrics(PANGO_FONT(font.get()), nullptr);
        const int width = pango_font_metrics_get_approximate_digit_width(metrics);
        pango_font_metrics_unref(metrics);
        return width;
    }
};

TEST_F(BundledFontTest, TheFourFileNamesAreInTheDocumentedOrder) {
    const std::vector<std::string> expected = {
        "FantasqueSansMono-Regular.ttf", "FantasqueSansMono-Bold.ttf",
        "FantasqueSansMono-Italic.ttf", "FantasqueSansMono-BoldItalic.ttf"};
    EXPECT_TRUE(bundled_font_files() == expected);
}

TEST_F(BundledFontTest, TheResourcesHoldTheVendoredFilesUnchanged) {
    for (const std::string& name : bundled_font_files()) {
        SCOPED_TRACE(name);
        bool found = false;
        const std::string in_program = resource_bytes(name, found);
        ASSERT_TRUE(found);
        const std::string on_disk = read_file(std::string(VERMILION_FONT_DIR) + "/" + name);
        ASSERT_FALSE(on_disk.empty());
        EXPECT_TRUE(in_program == on_disk);
    }
}

TEST_F(BundledFontTest, TheFolderIsUnderTheUserCacheFolder) {
    EXPECT_EQ(bundled_font_folder(), cache_root() + "/vermilion/fonts");
}

TEST_F(BundledFontTest, WritingMakesTheFourFilesWithTheResourceBytes) {
    const std::string folder = cache_root() + "/write-test/deep";
    const std::vector<std::string> paths = write_bundled_font(folder);
    ASSERT_EQ(paths.size(), std::size_t{4});
    for (std::size_t i = 0; i < paths.size(); ++i) {
        SCOPED_TRACE(paths[i]);
        EXPECT_EQ(paths[i], folder + "/" + bundled_font_files()[i]);
        bool found = false;
        EXPECT_TRUE(read_file(paths[i]) == resource_bytes(bundled_font_files()[i], found));
    }
    // A second call finds the files in place and returns the same paths.
    EXPECT_TRUE(write_bundled_font(folder) == paths);
}

TEST_F(BundledFontTest, WritingIntoAFolderThatCannotBeMadeReturnsNothing) {
    const std::string blocker = cache_root() + "/blocker";
    {
        std::ofstream out(blocker, std::ios::binary);
        out << "a file, not a folder";
    }
    EXPECT_TRUE(write_bundled_font(blocker + "/fonts").empty());
}

TEST_F(BundledFontTest, RegisteringNothingOrMissingFilesGivesZero) {
    EXPECT_EQ(register_font_files({}), 0);
    EXPECT_EQ(register_font_files({cache_root() + "/does/not/exist.ttf"}), 0);
}

TEST_F(BundledFontTest, InstallingRegistersAllFourFilesFromTheCacheFolder) {
    EXPECT_EQ(registered_count(), 4);
    for (const std::string& name : bundled_font_files()) {
        EXPECT_TRUE(fs::is_regular_file(bundled_font_folder() + "/" + name)) << name;
    }
    // The work is done once: the second call returns the first call's count.
    EXPECT_EQ(install_bundled_font(), 4);
}

TEST_F(BundledFontTest, TheFamilyIsListedMonospaceWithFourFaces) {
    ASSERT_EQ(registered_count(), 4);
    g_autofree PangoFontFamily** all = nullptr;
    int n = 0;
    PangoFontFamily* family = find_family(&all, &n);
    ASSERT_NE(family, nullptr);
    EXPECT_TRUE(pango_font_family_is_monospace(family));
    g_autofree PangoFontFace** faces = nullptr;
    int face_count = 0;
    pango_font_family_list_faces(family, &faces, &face_count);
    std::set<std::string> names;
    for (int i = 0; i < face_count; ++i) {
        names.insert(pango_font_face_get_face_name(faces[i]));
    }
    const std::set<std::string> expected = {"Regular", "Bold", "Italic", "Bold Italic"};
    EXPECT_TRUE(names == expected);
}

TEST_F(BundledFontTest, AllFourStylesHaveTheSameDigitWidth) {
    ASSERT_EQ(registered_count(), 4);
    std::string family;
    const int regular = digit_width(PANGO_WEIGHT_NORMAL, PANGO_STYLE_NORMAL, family);
    EXPECT_EQ(family, std::string(kBundledFontFamily));
    EXPECT_GT(regular, 0);
    EXPECT_EQ(digit_width(PANGO_WEIGHT_BOLD, PANGO_STYLE_NORMAL, family), regular);
    EXPECT_EQ(family, std::string(kBundledFontFamily));
    EXPECT_EQ(digit_width(PANGO_WEIGHT_NORMAL, PANGO_STYLE_ITALIC, family), regular);
    EXPECT_EQ(family, std::string(kBundledFontFamily));
    EXPECT_EQ(digit_width(PANGO_WEIGHT_BOLD, PANGO_STYLE_ITALIC, family), regular);
    EXPECT_EQ(family, std::string(kBundledFontFamily));
}

} // namespace
