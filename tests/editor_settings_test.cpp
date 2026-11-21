// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for EditorSettings: defaults, limits, the names, reading and writing settings.ini.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/colour_mode.h"
#include "core/editor_settings.h"
#include "core/settings_file.h"
#include "core/settings_keys.h"
#include "core/text_scan.h"
#include "settings_test_dir.h"

namespace {

using settings_test::SettingsDir;
using vermilion::ColourMode;
using vermilion::default_settings;
using vermilion::EditorSettings;
using vermilion::load_settings;
using vermilion::normalise_settings;
using vermilion::SettingsFile;
using vermilion::store_settings;

class EditorSettingsFileTest : public SettingsDir {
protected:
    // Reads the settings file from disk with a fresh SettingsFile.
    EditorSettings read_back() const {
        SettingsFile reader(path_);
        reader.load();
        return load_settings(reader);
    }
};

TEST(EditorSettingsDefaults, TheDocumentedDefaults) {
    const EditorSettings settings = default_settings();
    EXPECT_EQ(settings.colour_mode, ColourMode::System);
    EXPECT_TRUE(settings.highlight_current_line);
    EXPECT_EQ(settings.font, std::string("Fantasque Sans Mono 12"));
    EXPECT_EQ(settings.tab_width, 4);
    EXPECT_TRUE(settings.confirm_large_reload);
    EXPECT_EQ(settings.long_line_limit, std::uint64_t{350000});
    EXPECT_TRUE(settings == EditorSettings{});
}

TEST(EditorSettingsDefaults, TheLongLineDefaultIsTheScannersDefault) {
    EXPECT_EQ(default_settings().long_line_limit, vermilion::kDefaultLongLineLimit);
}

TEST(EditorSettingsDefaults, TheApprovedNames) {
    namespace names = vermilion::settings;
    EXPECT_EQ(std::string(names::kAppearanceGroup), std::string("appearance"));
    EXPECT_EQ(std::string(names::kColourMode), std::string("colour_mode"));
    EXPECT_EQ(std::string(names::kColourModeDefault), std::string("system"));
    EXPECT_EQ(std::string(names::kHighlightCurrentLine), std::string("highlight_current_line"));
    EXPECT_EQ(std::string(names::kEditorGroup), std::string("editor"));
    EXPECT_EQ(std::string(names::kFont), std::string("font"));
    EXPECT_EQ(std::string(names::kTabWidth), std::string("tab_width"));
    EXPECT_EQ(std::string(names::kFilesGroup), std::string("files"));
    EXPECT_EQ(std::string(names::kConfirmLargeReload), std::string("confirm_large_reload"));
    EXPECT_EQ(std::string(names::kLongLineLimit), std::string("long_line_limit"));
    EXPECT_EQ(names::kLongLineLimitMin, 10000);
    EXPECT_EQ(names::kLongLineLimitMax, 5000000);
}

TEST(EditorSettingsDefaults, EqualityLooksAtEveryField) {
    const EditorSettings base = default_settings();
    EditorSettings other = base;
    EXPECT_TRUE(other == base);
    EXPECT_FALSE(other != base);
    other.colour_mode = ColourMode::Dark;
    EXPECT_TRUE(other != base);
    other = base;
    other.highlight_current_line = false;
    EXPECT_TRUE(other != base);
    other = base;
    other.font = "Mono 10";
    EXPECT_TRUE(other != base);
    other = base;
    other.tab_width = 8;
    EXPECT_TRUE(other != base);
    other = base;
    other.confirm_large_reload = false;
    EXPECT_TRUE(other != base);
    other = base;
    other.long_line_limit = 20000;
    EXPECT_TRUE(other != base);
}

TEST(EditorSettingsNormalise, OnlyFourAndEightAreTabWidths) {
    for (int width : {0, 1, 2, 3, 5, 6, 7, 16, -4}) {
        EditorSettings settings;
        settings.tab_width = width;
        EXPECT_EQ(normalise_settings(settings).tab_width, 4);
    }
    EditorSettings four;
    four.tab_width = 4;
    EXPECT_EQ(normalise_settings(four).tab_width, 4);
    EditorSettings eight;
    eight.tab_width = 8;
    EXPECT_EQ(normalise_settings(eight).tab_width, 8);
}

TEST(EditorSettingsNormalise, TheLongLineLimitStaysBetweenItsBounds) {
    const std::uint64_t cases[][2] = {
        {0, 10000},       {1, 10000},         {9999, 10000},      {10000, 10000},
        {350000, 350000}, {5000000, 5000000}, {5000001, 5000000}, {UINT64_MAX, 5000000},
    };
    for (const auto& pair : cases) {
        EditorSettings settings;
        settings.long_line_limit = pair[0];
        EXPECT_EQ(normalise_settings(settings).long_line_limit, pair[1]);
    }
}

TEST(EditorSettingsNormalise, BlanksAroundTheFontAreCut) {
    EditorSettings settings;
    settings.font = "  DejaVu Sans Mono 11\t";
    EXPECT_EQ(normalise_settings(settings).font, std::string("DejaVu Sans Mono 11"));
    settings.font = " \t ";
    EXPECT_EQ(normalise_settings(settings).font, std::string(""));
    settings.font = "Noto  Sans Mono 10";
    EXPECT_EQ(normalise_settings(settings).font, std::string("Noto  Sans Mono 10"));
}

TEST(EditorSettingsNormalise, OtherFieldsAndASecondRunChangeNothing) {
    EditorSettings settings;
    settings.colour_mode = ColourMode::Dark;
    settings.highlight_current_line = false;
    settings.confirm_large_reload = false;
    settings.tab_width = 6;
    settings.long_line_limit = 3;
    settings.font = " x ";
    const EditorSettings once = normalise_settings(settings);
    EXPECT_EQ(once.colour_mode, ColourMode::Dark);
    EXPECT_FALSE(once.highlight_current_line);
    EXPECT_FALSE(once.confirm_large_reload);
    EXPECT_TRUE(normalise_settings(once) == once);
}

TEST_F(EditorSettingsFileTest, AMissingFileGivesTheDefaults) {
    SettingsFile file(path_);
    file.load();
    EXPECT_TRUE(load_settings(file) == default_settings());
}

TEST_F(EditorSettingsFileTest, EveryKeyIsRead) {
    put("[appearance]\ncolour_mode=dark\nhighlight_current_line=false\n"
        "[editor]\nfont=DejaVu Sans Mono 11\ntab_width=8\n"
        "[files]\nconfirm_large_reload=false\nlong_line_limit=20000\n");
    const EditorSettings settings = read_back();
    EXPECT_EQ(settings.colour_mode, ColourMode::Dark);
    EXPECT_FALSE(settings.highlight_current_line);
    EXPECT_EQ(settings.font, std::string("DejaVu Sans Mono 11"));
    EXPECT_EQ(settings.tab_width, 8);
    EXPECT_FALSE(settings.confirm_large_reload);
    EXPECT_EQ(settings.long_line_limit, std::uint64_t{20000});
}

TEST_F(EditorSettingsFileTest, AMissingFontLineGivesTheBundledFont) {
    put("[editor]\ntab_width=8\n");
    EXPECT_EQ(read_back().font, std::string("Fantasque Sans Mono 12"));
}

TEST_F(EditorSettingsFileTest, AnEmptyFontLineStaysTheSystemFont) {
    put("[editor]\nfont=\n");
    EXPECT_EQ(read_back().font, std::string(""));
}

TEST_F(EditorSettingsFileTest, ChoosingTheSystemFontIsWrittenAndKept) {
    SettingsFile file(path_);
    file.load();
    EditorSettings settings = default_settings();
    settings.font.clear();
    store_settings(settings, file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_TRUE(contains(text(), "font="));
    EXPECT_EQ(read_back().font, std::string(""));
}

TEST_F(EditorSettingsFileTest, ValuesOutsideTheRulesAreMovedIntoThem) {
    put("[editor]\ntab_width=5\n[files]\nlong_line_limit=5\n");
    EditorSettings settings = read_back();
    EXPECT_EQ(settings.tab_width, 4);
    EXPECT_EQ(settings.long_line_limit, std::uint64_t{10000});

    put("[files]\nlong_line_limit=9000000\n");
    EXPECT_EQ(read_back().long_line_limit, std::uint64_t{5000000});

    put("[files]\nlong_line_limit=-3\n");
    EXPECT_EQ(read_back().long_line_limit, std::uint64_t{10000});
}

TEST_F(EditorSettingsFileTest, TextThatIsNotAValueGivesTheDefault) {
    put("[appearance]\ncolour_mode=blue\nhighlight_current_line=maybe\n"
        "[editor]\ntab_width=abc\n[files]\nconfirm_large_reload=perhaps\n"
        "long_line_limit=abc\n");
    EXPECT_TRUE(read_back() == default_settings());

    put("[files]\nlong_line_limit=99999999999\n");
    EXPECT_EQ(read_back().long_line_limit, std::uint64_t{350000});
}

TEST_F(EditorSettingsFileTest, TheColourModeIgnoresCase) {
    put("[appearance]\ncolour_mode=DARK\n");
    EXPECT_EQ(read_back().colour_mode, ColourMode::Dark);
    put("[appearance]\ncolour_mode=Light\n");
    EXPECT_EQ(read_back().colour_mode, ColourMode::Light);
}

TEST_F(EditorSettingsFileTest, TheOldSpellingOfTheReloadKeyIsNotRead) {
    put("[files]\nconfirm-large-reload=false\n");
    EXPECT_TRUE(read_back().confirm_large_reload);
}

TEST_F(EditorSettingsFileTest, DefaultsOnAnEmptyFileWriteNothing) {
    SettingsFile file(path_);
    file.load();
    store_settings(default_settings(), file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_FALSE(folder_exists());
}

TEST_F(EditorSettingsFileTest, OnlyTheChangedKeysAreWritten) {
    SettingsFile file(path_);
    file.load();
    EditorSettings settings = default_settings();
    settings.tab_width = 8;
    store_settings(settings, file);
    ASSERT_TRUE(file.save().is_ok());
    const std::string written = text();
    EXPECT_TRUE(contains(written, "[editor]"));
    EXPECT_TRUE(contains(written, "tab_width=8"));
    EXPECT_FALSE(contains(written, "colour_mode"));
    EXPECT_FALSE(contains(written, "highlight_current_line"));
    EXPECT_FALSE(contains(written, "font"));
    EXPECT_FALSE(contains(written, "confirm_large_reload"));
    EXPECT_FALSE(contains(written, "long_line_limit"));
}

TEST_F(EditorSettingsFileTest, EverySettingRoundTrips) {
    EditorSettings settings;
    settings.colour_mode = ColourMode::Dark;
    settings.highlight_current_line = false;
    settings.font = "DejaVu Sans Mono 11";
    settings.tab_width = 8;
    settings.confirm_large_reload = false;
    settings.long_line_limit = 1000000;
    SettingsFile file(path_);
    file.load();
    store_settings(settings, file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_TRUE(contains(text(), "colour_mode=dark"));
    EXPECT_TRUE(read_back() == settings);
}

TEST_F(EditorSettingsFileTest, TheColourModeWordsAreWritten) {
    for (const char* word : {"light", "dark"}) {
        EditorSettings settings;
        settings.colour_mode = vermilion::parse_colour_mode(word);
        SettingsFile file(path_);
        file.load();
        store_settings(settings, file);
        ASSERT_TRUE(file.save().is_ok());
        EXPECT_TRUE(contains(text(), std::string("colour_mode=") + word));
    }
}

TEST_F(EditorSettingsFileTest, ChangingBackToTheDefaultIsWrittenOut) {
    put("[editor]\ntab_width=8\n");
    SettingsFile file(path_);
    file.load();
    store_settings(default_settings(), file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_TRUE(contains(text(), "tab_width=4"));
    EXPECT_FALSE(contains(text(), "tab_width=8"));
    EXPECT_EQ(read_back().tab_width, 4);
}

TEST_F(EditorSettingsFileTest, ValuesAreNormalisedBeforeTheyAreWritten) {
    SettingsFile file(path_);
    file.load();
    EditorSettings settings = default_settings();
    settings.tab_width = 6;
    settings.long_line_limit = 1;
    settings.font = " Mono 10 ";
    store_settings(settings, file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_TRUE(contains(text(), "long_line_limit=10000"));
    EXPECT_TRUE(contains(text(), "font=Mono 10"));
    EXPECT_FALSE(contains(text(), "tab_width"));
}

TEST_F(EditorSettingsFileTest, UnknownKeysSurviveAStore) {
    put("[editor]\nfuture=abc\n[extra]\nkept=yes\n");
    SettingsFile file(path_);
    file.load();
    EditorSettings settings = default_settings();
    settings.tab_width = 8;
    store_settings(settings, file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_TRUE(contains(text(), "future=abc"));
    EXPECT_TRUE(contains(text(), "kept=yes"));
    EXPECT_TRUE(contains(text(), "tab_width=8"));
}

TEST_F(EditorSettingsFileTest, AStoreOfTheSameValuesAgainRemembersNothing) {
    put("[editor]\ntab_width=8\n[appearance]\ncolour_mode=blue\n");
    SettingsFile file(path_);
    file.load();
    const std::string before = text();
    store_settings(load_settings(file), file);
    ASSERT_TRUE(file.save().is_ok());
    EXPECT_EQ(text(), before);
}

TEST_F(EditorSettingsFileTest, TwoProgramsWritingDifferentKeysKeepBoth) {
    SettingsFile first(path_);
    SettingsFile second(path_);
    first.load();
    second.load();
    EditorSettings a = default_settings();
    a.tab_width = 8;
    EditorSettings b = default_settings();
    b.colour_mode = ColourMode::Dark;
    store_settings(a, first);
    store_settings(b, second);
    ASSERT_TRUE(first.save().is_ok());
    ASSERT_TRUE(second.save().is_ok());
    const EditorSettings result = read_back();
    EXPECT_EQ(result.tab_width, 8);
    EXPECT_EQ(result.colour_mode, ColourMode::Dark);
}

} // namespace
