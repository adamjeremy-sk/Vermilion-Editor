// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Helpers shared by the recovery store tests: file helpers and a fixture with a temporary root.

#pragma once

#include <gtest/gtest.h>

#include <glib.h>
#include <sys/stat.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/recovery_store.h"

namespace store_test {

namespace fs = std::filesystem;
using Names = std::vector<std::string>;

inline void put(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

inline std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The names in a folder (links not followed), in byte order.
inline Names names_in(const std::string& folder) {
    Names names;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, ec)) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// The permission bits of a path (a link is not followed); 0 when it is not there.
inline unsigned mode_of(const std::string& path) {
    struct stat info;
    if (lstat(path.c_str(), &info) != 0) {
        return 0;
    }
    return info.st_mode & 07777;
}

// A temporary directory as the root of the recovery folders, removed after the test.
class RecoveryStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_autofree char* dir = g_dir_make_tmp("vermilion-store-XXXXXX", nullptr);
        ASSERT_TRUE(dir != nullptr);
        root_ = dir;
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    vermilion::RecoveryStore light() const {
        return vermilion::RecoveryStore(root_, vermilion::RecoveryProgram::Light);
    }
    std::string folder() const { return root_ + "/light"; }
    std::string in_folder(const std::string& name) const { return folder() + "/" + name; }
    void make_folder() const { fs::create_directories(folder()); }

    std::string root_;
};

} // namespace store_test
