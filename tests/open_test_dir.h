// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A scratch folder for the tests of the command-line arguments: files, folders and names.

#pragma once

#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace open_test {

namespace fs = std::filesystem;

// Each test gets its own empty folder under the system's temporary folder, removed afterwards.
class OpenTestDir : public ::testing::Test {
protected:
    void SetUp() override {
        static int counter = 0;
        root_ = fs::temp_directory_path() /
                ("vermilion_open_" + std::to_string(::getpid()) + "_" + std::to_string(++counter));
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    // The path of `name` inside the folder; nothing is created.
    std::string path_of(const std::string& name) const { return (root_ / name).string(); }

    // Creates a small file and returns its path.
    std::string make_file(const std::string& name) const {
        std::ofstream out(root_ / name);
        out << "text\n";
        return path_of(name);
    }

    // Creates a folder and returns its path.
    std::string make_dir(const std::string& name) const {
        std::error_code ec;
        fs::create_directory(root_ / name, ec);
        return path_of(name);
    }

    fs::path root_;
};

} // namespace open_test
