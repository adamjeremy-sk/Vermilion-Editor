// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A scratch folder with a settings.ini path, for the tests of the settings.

#pragma once

#include <gtest/gtest.h>

#include <glib.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace settings_test {

namespace fs = std::filesystem;

// Each test gets its own empty folder, removed afterwards. The settings folder inside it does not
// exist yet, like a first run: only a save() creates it.
class SettingsDir : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* made = g_dir_make_tmp("vermilion-settings-XXXXXX", nullptr);
        ASSERT_NE(made, nullptr);
        root_ = made;
        folder_ = root_ + "/vermilion";
        path_ = folder_ + "/settings.ini";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    // Writes `content` as the settings file, creating the folder.
    void put(const std::string& content) const {
        std::error_code ec;
        fs::create_directories(folder_, ec);
        std::ofstream out(path_, std::ios::binary);
        out << content;
    }

    // The text of the settings file; empty when there is none.
    std::string text() const {
        std::ifstream in(path_, std::ios::binary);
        std::ostringstream all;
        all << in.rdbuf();
        return all.str();
    }

    bool file_exists() const {
        std::error_code ec;
        return fs::exists(path_, ec);
    }

    bool folder_exists() const {
        std::error_code ec;
        return fs::exists(folder_, ec);
    }

    static bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    std::string root_;
    std::string folder_;
    std::string path_;
};

} // namespace settings_test
