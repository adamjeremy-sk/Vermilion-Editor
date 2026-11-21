// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for SettingsFile and settings_keys: load, typed reads, save, damaged files, two writers.

#include <gtest/gtest.h>

#include <glib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/settings_file.h"
#include "core/settings_keys.h"

namespace {

namespace fs = std::filesystem;

using vermilion::SettingsFile;
using vermilion::SettingsLoad;
using vermilion::Status;

void put(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The names in a folder, in byte order.
std::vector<std::string> names_in(const std::string& folder) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, ec)) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

class SettingsFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* made = g_dir_make_tmp("vermilion-settings-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        root_ = made;
        // The settings folder does not exist yet, like a first run: save() must create it.
        folder_ = root_ + "/vermilion";
        path_ = folder_ + "/settings.ini";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    void make_folder() { fs::create_directories(folder_); }

    std::string root_;
    std::string folder_;
    std::string path_;
};

TEST(SettingsKeys, TheApprovedNames) {
    EXPECT_EQ(std::string(vermilion::settings::kFilesGroup), std::string("files"));
    EXPECT_EQ(std::string(vermilion::settings::kConfirmLargeReload),
              std::string("confirm_large_reload"));
    EXPECT_TRUE(vermilion::settings::kConfirmLargeReloadDefault);
}

TEST(SettingsFilePath, DefaultPathIsInTheVermilionFolder) {
    const std::string path = SettingsFile::default_path();
    const std::string tail = "/vermilion/settings.ini";
    ASSERT_GE(path.size(), tail.size());
    EXPECT_EQ(path.substr(path.size() - tail.size()), tail);
    EXPECT_EQ(path.front(), '/');
}

TEST_F(SettingsFileTest, MissingFileGivesTheDefaultsAndCreatesNothing) {
    SettingsFile settings(path_);
    EXPECT_EQ(settings.load(), SettingsLoad::Missing);
    EXPECT_EQ(settings.state(), SettingsLoad::Missing);
    EXPECT_TRUE(settings.get_bool("files", "confirm_large_reload", true));
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", false));
    EXPECT_FALSE(fs::exists(root_ + "/vermilion"));
}

TEST_F(SettingsFileTest, SaveWithNothingChangedWritesNothing) {
    SettingsFile settings(path_);
    settings.load();
    EXPECT_TRUE(settings.save().is_ok());
    EXPECT_FALSE(fs::exists(root_ + "/vermilion"));
}

TEST_F(SettingsFileTest, SetIsVisibleBeforeSave) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_bool("files", "confirm_large_reload", false);
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", true));
    EXPECT_FALSE(fs::exists(path_));
}

TEST_F(SettingsFileTest, SaveCreatesTheFolderAndTheFileAndItReadsBack) {
    SettingsFile writer(path_);
    writer.load();
    writer.set_bool("files", "confirm_large_reload", false);
    ASSERT_TRUE(writer.save().is_ok());
    EXPECT_EQ(writer.state(), SettingsLoad::Loaded);
    EXPECT_TRUE(fs::exists(path_));

    SettingsFile reader(path_);
    EXPECT_EQ(reader.load(), SettingsLoad::Loaded);
    EXPECT_FALSE(reader.get_bool("files", "confirm_large_reload", true));
}

TEST_F(SettingsFileTest, TheFileHoldsTheGroupAndTheKey) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_bool("files", "confirm_large_reload", true);
    ASSERT_TRUE(settings.save().is_ok());
    const std::string text = slurp(path_);
    EXPECT_TRUE(contains(text, "[files]"));
    EXPECT_TRUE(contains(text, "confirm_large_reload=true"));
}

TEST_F(SettingsFileTest, SaveLeavesOnlyTheSettingsFile) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_bool("files", "confirm_large_reload", false);
    ASSERT_TRUE(settings.save().is_ok());
    settings.set_bool("files", "confirm_large_reload", true);
    ASSERT_TRUE(settings.save().is_ok());
    EXPECT_EQ(names_in(folder_), std::vector<std::string>{"settings.ini"});
    SettingsFile reader(path_);
    reader.load();
    EXPECT_TRUE(reader.get_bool("files", "confirm_large_reload", false));
}

TEST_F(SettingsFileTest, EmptyFileIsLoadedWithTheDefaults) {
    make_folder();
    put(path_, "");
    SettingsFile settings(path_);
    EXPECT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_TRUE(settings.get_bool("files", "confirm_large_reload", true));
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", false));
}

TEST_F(SettingsFileTest, UnknownGroupsKeysAndCommentsAreKept) {
    make_folder();
    put(path_, "# written by hand\n[window]\nwidth=800\n# the height\nheight=600\n\n[files]\n"
               "confirm_large_reload=false\nfuture-key=abc\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    settings.set_bool("files", "confirm_large_reload", true);
    ASSERT_TRUE(settings.save().is_ok());
    const std::string text = slurp(path_);
    EXPECT_TRUE(contains(text, "# written by hand"));
    EXPECT_TRUE(contains(text, "[window]"));
    EXPECT_TRUE(contains(text, "width=800"));
    EXPECT_TRUE(contains(text, "# the height"));
    EXPECT_TRUE(contains(text, "height=600"));
    EXPECT_TRUE(contains(text, "future-key=abc"));
    EXPECT_TRUE(contains(text, "confirm_large_reload=true"));
    EXPECT_FALSE(contains(text, "confirm_large_reload=false"));
}

TEST_F(SettingsFileTest, WrongValueTypeGivesTheFallback) {
    make_folder();
    put(path_, "[files]\nconfirm_large_reload=maybe\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_TRUE(settings.get_bool("files", "confirm_large_reload", true));
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", false));
}

TEST_F(SettingsFileTest, ZeroAndOneAreBooleans) {
    make_folder();
    put(path_, "[test]\na=1\nb=0\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_TRUE(settings.get_bool("test", "a", false));
    EXPECT_FALSE(settings.get_bool("test", "b", true));
}

TEST_F(SettingsFileTest, DamagedFileGivesTheDefaultsAndIsLeftAlone) {
    make_folder();
    const std::string junk = "this is not [[ an ini file\n=== nothing\n";
    put(path_, junk);
    SettingsFile settings(path_);
    EXPECT_EQ(settings.load(), SettingsLoad::Damaged);
    EXPECT_TRUE(settings.get_bool("files", "confirm_large_reload", true));
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", false));
    EXPECT_TRUE(settings.save().is_ok()); // nothing changed: nothing written
    EXPECT_EQ(slurp(path_), junk);
    EXPECT_EQ(names_in(folder_), std::vector<std::string>{"settings.ini"});
}

TEST_F(SettingsFileTest, DamagedFileIsMovedAsideWhenASettingIsSaved) {
    make_folder();
    const std::string junk = "this is not [[ an ini file\n";
    put(path_, junk);
    put(path_ + ".bad", "an older bad file");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Damaged);
    settings.set_bool("files", "confirm_large_reload", false);
    ASSERT_TRUE(settings.save().is_ok());
    EXPECT_EQ(settings.state(), SettingsLoad::Loaded);
    EXPECT_EQ(slurp(path_ + ".bad"), junk);
    EXPECT_EQ(names_in(folder_), (std::vector<std::string>{"settings.ini", "settings.ini.bad"}));
    SettingsFile reader(path_);
    EXPECT_EQ(reader.load(), SettingsLoad::Loaded);
    EXPECT_FALSE(reader.get_bool("files", "confirm_large_reload", true));
}

TEST_F(SettingsFileTest, TwoWritersKeepEachOthersKeys) {
    make_folder();
    put(path_, "[files]\nother=1\n");
    SettingsFile first(path_);
    SettingsFile second(path_);
    ASSERT_EQ(first.load(), SettingsLoad::Loaded);
    ASSERT_EQ(second.load(), SettingsLoad::Loaded);
    first.set_bool("test", "a", true);
    second.set_bool("test", "b", true);
    ASSERT_TRUE(first.save().is_ok());
    ASSERT_TRUE(second.save().is_ok()); // reads the file again, so the first writer's key stays
    SettingsFile reader(path_);
    ASSERT_EQ(reader.load(), SettingsLoad::Loaded);
    EXPECT_TRUE(reader.get_bool("test", "a", false));
    EXPECT_TRUE(reader.get_bool("test", "b", false));
    EXPECT_TRUE(reader.get_bool("files", "other", false));
    // The second writer sees the first one's key after its own save.
    EXPECT_TRUE(second.get_bool("test", "a", false));
}

TEST_F(SettingsFileTest, SaveFailsWhenThePathRunsThroughAFile) {
    put(root_ + "/blocker", "a plain file where a folder is needed");
    SettingsFile settings(root_ + "/blocker/vermilion/settings.ini");
    settings.load();
    settings.set_bool("files", "confirm_large_reload", false);
    Status status = settings.save();
    ASSERT_FALSE(status.is_ok());
    EXPECT_FALSE(status.error().message.empty());
    // The change is still held in memory, and a later save can be tried again.
    EXPECT_FALSE(settings.get_bool("files", "confirm_large_reload", true));
}

TEST_F(SettingsFileTest, LoadAgainDropsPendingChanges) {
    make_folder();
    put(path_, "[files]\nconfirm_large_reload=true\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    settings.set_bool("files", "confirm_large_reload", false);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_TRUE(settings.get_bool("files", "confirm_large_reload", false));
    EXPECT_TRUE(settings.save().is_ok()); // nothing remembered any more
    EXPECT_TRUE(contains(slurp(path_), "confirm_large_reload=true"));
}

TEST(SettingsFileMessages, UnusableMessageNamesThePath) {
    EXPECT_EQ(vermilion::settings_unusable_message("/x/settings.ini"),
              std::string("Could not read /x/settings.ini; using the default settings."));
}

} // namespace
