// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for File > Reload from Disk in ExternalChangeController: real files, real inspections and
// scans, a recording fake host.

#include <gtest/gtest.h>

#include <gio/gio.h>
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/document_list.h"
#include "core/external_change.h"
#include "core/external_messages.h"
#include "core/file_hash.h"
#include "core/open_messages.h"
#include "core/text_format.h"
#include "ui/external_change_controller.h"
#include "ui/glib_timer.h"

namespace {

namespace fs = std::filesystem;

using vermilion::Baseline;
using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::FileStamp;
using vermilion::OpenWarning;
using vermilion::ReloadAsk;
using vermilion::ScanResult;
using vermilion::TextFormat;
using vermilion::ui::ExternalChangeController;
using vermilion::ui::ExternalChangeHost;
using vermilion::ui::GlibTimer;

bool pump_until(const std::function<bool()>& done) {
    gint64 end = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    return done();
}

void pump_for(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

class TempDir {
public:
    TempDir() {
        char pattern[] = "/tmp/vermilion-reload-XXXXXX";
        const char* made = mkdtemp(pattern);
        dir_ = made != nullptr ? made : "";
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    std::string path(const std::string& name) const { return dir_ + "/" + name; }

private:
    std::string dir_;
};

void write_file(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// A text file just over the large-file limit.
std::string large_text() {
    std::string text;
    const std::string line = "abcd\n";
    text.reserve(vermilion::kLargeFileBytes + line.size());
    while (text.size() <= vermilion::kLargeFileBytes) {
        text += line;
    }
    return text;
}

FileStamp stamp_of(const std::string& path) {
    FileStamp stamp;
    GFile* file = g_file_new_for_path(path.c_str());
    GFileInfo* info =
        g_file_query_info(file,
                          G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_TIME_MODIFIED
                                                         "," G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
                          G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    if (info != nullptr) {
        stamp.size = g_file_info_get_size(info);
        GDateTime* time = g_file_info_get_modification_date_time(info);
        stamp.mtime_us = g_date_time_to_unix_usec(time);
        g_date_time_unref(time);
        g_object_unref(info);
    }
    g_object_unref(file);
    vermilion::Result<vermilion::FileHash> hashed = vermilion::hash_file_blocking(path);
    if (hashed.is_ok()) {
        stamp.hash = hashed.value().hash;
    }
    return stamp;
}

struct Reload {
    DocumentId id = 0;
    std::string message;
    ScanResult scan;
};

struct Question {
    std::string message;
    std::string detail;
    std::string go_label;
    bool offer_dont_ask = false;
    std::function<void(bool)> go;
};

struct OpenAsk {
    std::string message;
    std::function<void()> go;
};

// A host that records what the controller asks of it.
class ReloadHost : public ExternalChangeHost {
public:
    std::optional<PageState> page_state(DocumentId id) const override {
        auto found = pages.find(id);
        if (found == pages.end()) {
            return std::nullopt;
        }
        return found->second;
    }
    void set_baseline(DocumentId, const FileStamp&) override { ++other_calls; }
    void show_banner(DocumentId, const std::string&, const std::string&, const std::string&,
                     std::function<void()>, std::function<void()>) override {
        ++other_calls;
    }
    void hide_banner(DocumentId) override {}
    bool reload(DocumentId id, const ScanResult& scan, const std::string& text) override {
        reloads.push_back({id, text, scan});
        return true;
    }
    void mark_modified(DocumentId) override { ++other_calls; }
    void deleted_on_disk(DocumentId) override { ++other_calls; }
    void ask_reload_anyway(const std::string& text, const std::string&,
                           std::function<void()> go) override {
        open_asks.push_back({text, std::move(go)});
    }
    void ask_reload(const std::string& text, const std::string& detail, const std::string& label,
                    bool offer, std::function<void(bool)> go) override {
        questions.push_back({text, detail, label, offer, std::move(go)});
    }
    bool confirm_large_reload() const override { return confirm; }
    void set_confirm_large_reload(bool value) override { settings_set.push_back(value); }
    void message(const std::string& text) override { messages.push_back(text); }

    std::map<DocumentId, PageState> pages;
    std::vector<Reload> reloads;
    std::vector<Question> questions;
    std::vector<OpenAsk> open_asks;
    std::vector<bool> settings_set;
    std::vector<std::string> messages;
    bool confirm = true;
    int other_calls = 0;
};

class ExternalChangeReloadTest : public ::testing::Test {
protected:
    ExternalChangeReloadTest()
        : controller_(std::make_unique<ExternalChangeController>(
              list_, host_, std::make_unique<GlibTimer>(), 40)) {}

    // A file with `text`, a document for it and a quiet page whose baseline is the file as it is.
    DocumentId open(const std::string& name, const std::string& text) {
        const std::string path = dir_.path(name);
        write_file(path, text);
        DocumentId id = list_.add_file(path, name, TextFormat{}, false);
        ExternalChangeHost::PageState state;
        state.quiet = true;
        state.baseline = Baseline{path, stamp_of(path)};
        host_.pages[id] = state;
        return id;
    }

    // Asks for the reload and waits until the controller has nothing under way: it reloaded, or it
    // showed its question, or it said why not.
    void reload_by_hand(DocumentId id) {
        controller_->reload_from_disk(id);
        ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    }

    // Answers the question that was shown last (the user chose the go button) and waits.
    void answer(bool dont_ask) {
        ASSERT_FALSE(host_.questions.empty());
        host_.questions.back().go(dont_ask);
        ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    }

    std::string path_of(DocumentId id) const { return list_.find(id)->path; }

    TempDir dir_;
    DocumentList list_;
    ReloadHost host_;
    std::unique_ptr<ExternalChangeController> controller_;
};

TEST_F(ExternalChangeReloadTest, ACleanFileReloadsAtOnceEvenWhenItDidNotChange) {
    DocumentId id = open("a.txt", "one\n");
    reload_by_hand(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].id, id);
    EXPECT_EQ(host_.reloads[0].message, vermilion::reloaded_by_hand_message("a.txt"));
    EXPECT_EQ(host_.reloads[0].scan.size, 4u);
    EXPECT_TRUE(host_.questions.empty());
    EXPECT_TRUE(host_.messages.empty());
}

TEST_F(ExternalChangeReloadTest, TheReloadReadsTheNewContent) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    reload_by_hand(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].scan.size, 8u);
}

TEST_F(ExternalChangeReloadTest, APageWithoutABaselineStillReloads) {
    DocumentId id = open("a.txt", "one\n");
    host_.pages[id].baseline.reset();
    reload_by_hand(id);
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeReloadTest, AnEditedDocumentAsksToDiscardFirst) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    const vermilion::ReloadQuestion expected =
        vermilion::reload_question("a.txt", ReloadAsk::DiscardChanges, 4);
    EXPECT_EQ(host_.questions[0].message, expected.message);
    EXPECT_EQ(host_.questions[0].detail, expected.detail);
    EXPECT_EQ(host_.questions[0].go_label, expected.go);
    EXPECT_FALSE(host_.questions[0].offer_dont_ask);
    EXPECT_TRUE(host_.reloads.empty());
    answer(false);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].message, vermilion::reloaded_by_hand_message("a.txt"));
    EXPECT_TRUE(host_.settings_set.empty());
}

TEST_F(ExternalChangeReloadTest, CancellingTheQuestionLeavesEverythingAlone) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    pump_for(50);
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_FALSE(controller_->busy());
    reload_by_hand(id);
    EXPECT_EQ(host_.questions.size(), 2u);
}

TEST_F(ExternalChangeReloadTest, ATickOnTheDiscardQuestionDoesNotChangeTheSetting) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    reload_by_hand(id);
    answer(true);
    EXPECT_EQ(host_.reloads.size(), 1u);
    EXPECT_TRUE(host_.settings_set.empty());
}

TEST_F(ExternalChangeReloadTest, ALargeCleanFileAsksWithTheCheckBox) {
    DocumentId id = open("big.txt", large_text());
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    EXPECT_EQ(host_.questions[0].message,
              vermilion::reload_question("big.txt", ReloadAsk::LargeFile, 0).message);
    EXPECT_TRUE(host_.questions[0].offer_dont_ask);
    EXPECT_EQ(host_.questions[0].go_label, std::string("Reload"));
    EXPECT_TRUE(host_.reloads.empty());
    answer(false);
    EXPECT_EQ(host_.reloads.size(), 1u);
    EXPECT_TRUE(host_.settings_set.empty());
}

TEST_F(ExternalChangeReloadTest, TickingTheBoxTurnsTheSettingOffAndReloads) {
    DocumentId id = open("big.txt", large_text());
    reload_by_hand(id);
    answer(true);
    EXPECT_EQ(host_.settings_set, std::vector<bool>{false});
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeReloadTest, ALargeCleanFileReloadsAtOnceWhenTheSettingIsOff) {
    host_.confirm = false;
    DocumentId id = open("big.txt", large_text());
    reload_by_hand(id);
    EXPECT_TRUE(host_.questions.empty());
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeReloadTest, ALargeEditedFileGetsTheDiscardQuestionWithoutTheBox) {
    DocumentId id = open("big.txt", large_text());
    list_.set_modified(id, true);
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    EXPECT_EQ(host_.questions[0].go_label, std::string("Discard and Reload"));
    EXPECT_FALSE(host_.questions[0].offer_dont_ask);
}

TEST_F(ExternalChangeReloadTest, AMissingFileSaysSoAndNothingElseHappens) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    reload_by_hand(id);
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_missing_message("a.txt"));
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.questions.empty());
    EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    EXPECT_EQ(host_.other_calls, 0);
}

TEST_F(ExternalChangeReloadTest, AnUnknownDocumentAndAnUntitledOneDoNothing) {
    controller_->reload_from_disk(99);
    DocumentId untitled = list_.add_untitled();
    ExternalChangeHost::PageState state;
    state.quiet = true;
    host_.pages[untitled] = state;
    controller_->reload_from_disk(untitled);
    EXPECT_FALSE(controller_->busy());
    EXPECT_TRUE(host_.messages.empty());
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeReloadTest, ABusyPageSaysSo) {
    DocumentId id = open("a.txt", "one\n");
    host_.pages[id].quiet = false;
    controller_->reload_from_disk(id);
    EXPECT_FALSE(controller_->busy());
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_busy_message("a.txt"));
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeReloadTest, ASecondRequestWhileOneIsUnderWaySaysSo) {
    DocumentId id = open("a.txt", "one\n");
    controller_->reload_from_disk(id);
    controller_->reload_from_disk(id);
    ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    pump_for(50);
    EXPECT_EQ(host_.reloads.size(), 1u);
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_busy_message("a.txt"));
}

TEST_F(ExternalChangeReloadTest, AFileWithANulAsksTheOpenQuestionAgainThenReloads) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string("a\0b\n", 4));
    reload_by_hand(id);
    ASSERT_EQ(host_.open_asks.size(), 1u);
    EXPECT_EQ(host_.open_asks[0].message,
              vermilion::open_question("a.txt", OpenWarning::Binary, 0).message);
    EXPECT_TRUE(host_.reloads.empty());
    host_.open_asks[0].go();
    ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_TRUE(host_.reloads[0].scan.binary);
    EXPECT_EQ(host_.reloads[0].message, vermilion::reloaded_by_hand_message("a.txt"));
}

TEST_F(ExternalChangeReloadTest, AnOtherEncodingIsAnnouncedInTheLineOfTheReload) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "caf\xE9\n");
    reload_by_hand(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].message,
              vermilion::reloaded_by_hand_encoding_message("a.txt", "Windows-1252"));
}

TEST_F(ExternalChangeReloadTest, TheNextCheckOfTheFileUsesTheUsualWording) {
    DocumentId id = open("a.txt", "one\n");
    reload_by_hand(id);
    write_file(path_of(id), "one\ntwo\n");
    controller_->check(id);
    ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    ASSERT_EQ(host_.reloads.size(), 2u);
    EXPECT_EQ(host_.reloads[1].message, vermilion::reloaded_message("a.txt"));
}

TEST_F(ExternalChangeReloadTest, AnAnswerAfterTheControllerIsGoneIsIgnored) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    std::function<void(bool)> go = host_.questions[0].go;
    host_.questions.clear();
    controller_.reset();
    go(true);
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.settings_set.empty());
}

TEST_F(ExternalChangeReloadTest, ASaveAsWhileTheFileIsLookedAtEndsTheReload) {
    DocumentId id = open("a.txt", "one\n");
    controller_->reload_from_disk(id);
    list_.set_path(id, dir_.path("b.txt"), "b.txt");
    ASSERT_TRUE(pump_until([this] { return !controller_->busy(); }));
    pump_for(50);
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.questions.empty());
    EXPECT_TRUE(host_.messages.empty());
}

TEST_F(ExternalChangeReloadTest, AFileThatCannotBeReadSaysSoAndTheNextReloadWorks) {
    if (geteuid() == 0) {
        return; // the superuser reads any file
    }
    DocumentId id = open("a.txt", "one\n");
    ASSERT_EQ(chmod(path_of(id).c_str(), 0), 0);
    reload_by_hand(id);
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_failed_message("a.txt"));
    EXPECT_TRUE(host_.reloads.empty());
    ASSERT_EQ(chmod(path_of(id).c_str(), 0644), 0);
    reload_by_hand(id);
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeReloadTest, AnAnswerWhileThePageIsBusyOnlySaysSo) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    reload_by_hand(id);
    ASSERT_EQ(host_.questions.size(), 1u);
    host_.pages[id].quiet = false;
    answer(false);
    EXPECT_TRUE(host_.reloads.empty());
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_busy_message("a.txt"));
    EXPECT_FALSE(controller_->busy());
}

TEST_F(ExternalChangeReloadTest, APathThatCannotBeLookedAtSaysSo) {
    write_file(dir_.path("blocker"), "a plain file where a folder should be");
    DocumentId id = list_.add_file(dir_.path("blocker/a.txt"), "a.txt", TextFormat{}, false);
    ExternalChangeHost::PageState state;
    state.quiet = true;
    host_.pages[id] = state;
    reload_by_hand(id);
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::reload_failed_message("a.txt"));
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.questions.empty());
}

} // namespace
