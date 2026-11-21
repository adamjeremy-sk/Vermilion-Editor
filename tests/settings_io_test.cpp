// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of reading settings.ini again and saving what the Settings window applied.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/colour_mode.h"
#include "core/editor_settings.h"
#include "core/settings_file.h"
#include "core/settings_io.h"
#include "settings_test_dir.h"

namespace {

using settings_test::SettingsDir;
using vermilion::ColourMode;
using vermilion::default_settings;
using vermilion::EditorSettings;
using vermilion::load_settings;
using vermilion::reread_settings;
using vermilion::save_settings;
using vermilion::SettingsFile;

class SettingsIoTest : public SettingsDir {
protected:
    // Every field different from its default.
    static EditorSettings changed() {
        EditorSettings settings = default_settings();
        settings.colour_mode = ColourMode::Dark;
        settings.highlight_current_line = !settings.highlight_current_line;
        settings.font = "Foo Mono 13";
        settings.tab_width = 8;
        settings.confirm_large_reload = !settings.confirm_large_reload;
        settings.long_line_limit = 20000;
        return settings;
    }
};

TEST_F(SettingsIoTest, AMissingFileGivesTheDefaults) {
    SettingsFile file(path_);
    EXPECT_TRUE(reread_settings(file) == default_settings());
}

TEST_F(SettingsIoTest, ADamagedFileGivesTheDefaults) {
    put("this is not a settings file\n");
    SettingsFile file(path_);
    EXPECT_TRUE(reread_settings(file) == default_settings());
}

TEST_F(SettingsIoTest, SavedSettingsAreReadBackFromDisk) {
    SettingsFile writer(path_);
    ASSERT_TRUE(save_settings(changed(), writer).is_ok());
    SettingsFile reader(path_);
    EXPECT_TRUE(reread_settings(reader) == changed());
}

TEST_F(SettingsIoTest, AnotherProgramsChangeIsSeenAtTheNextRead) {
    SettingsFile ours(path_);
    EXPECT_TRUE(reread_settings(ours) == default_settings());
    SettingsFile theirs(path_);
    theirs.load();
    ASSERT_TRUE(save_settings(changed(), theirs).is_ok());
    EXPECT_TRUE(reread_settings(ours) == changed());
}

TEST_F(SettingsIoTest, TheFileObjectHoldsWhatWasSaved) {
    SettingsFile file(path_);
    reread_settings(file);
    ASSERT_TRUE(save_settings(changed(), file).is_ok());
    EXPECT_TRUE(load_settings(file) == changed());
}

TEST_F(SettingsIoTest, OnlyTheKeysThatDifferAreWritten) {
    SettingsFile file(path_);
    reread_settings(file);
    EditorSettings settings = default_settings();
    settings.colour_mode = ColourMode::Dark;
    ASSERT_TRUE(save_settings(settings, file).is_ok());
    const std::string written = text();
    EXPECT_NE(written.find("colour_mode"), std::string::npos);
    EXPECT_EQ(written.find("tab_width"), std::string::npos);
    EXPECT_EQ(written.find("long_line_limit"), std::string::npos);
}

TEST_F(SettingsIoTest, KeysThatTheProgramDoesNotKnowStay) {
    put("[ide]\nsomething=kept\n");
    SettingsFile file(path_);
    reread_settings(file);
    ASSERT_TRUE(save_settings(changed(), file).is_ok());
    EXPECT_NE(text().find("something=kept"), std::string::npos);
}

TEST_F(SettingsIoTest, NothingIsWrittenWhenNothingDiffers) {
    SettingsFile file(path_);
    reread_settings(file);
    EXPECT_TRUE(save_settings(default_settings(), file).is_ok());
    EXPECT_FALSE(std::filesystem::exists(path_));
}

TEST_F(SettingsIoTest, ASaveThatFailsSaysSoAndTheProgramKeepsTheValues) {
    // The settings folder cannot be made: a file has its name.
    {
        std::ofstream blocker(folder_, std::ios::binary);
        blocker << "x";
    }
    SettingsFile file(path_);
    reread_settings(file);
    EXPECT_FALSE(save_settings(changed(), file).is_ok());
    EXPECT_TRUE(load_settings(file) == changed());
}

} // namespace
