// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for ExternalChangeController: real files, real inspections and scans, a recording fake
// host.

#include <gtest/gtest.h>

#include <fcntl.h>
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

using vermilion::BannerKind;
using vermilion::Baseline;
using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::FileStamp;
using vermilion::OpenWarning;
using vermilion::ScanResult;
using vermilion::TextEncoding;
using vermilion::TextFormat;
using vermilion::ui::ExternalChangeController;
using vermilion::ui::ExternalChangeHost;
using vermilion::ui::GlibTimer;

// Runs the default main context until done() holds or 5 seconds pass; true when done() holds.
bool pump_until(const std::function<bool()>& done) {
    gint64 end = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    return done();
}

// Runs the main context for `ms` milliseconds.
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
        char pattern[] = "/tmp/vermilion-extctl-XXXXXX";
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

// Moves the file's modified time to `seconds` (since the epoch) without touching its bytes.
void set_time(const std::string& path, long seconds) {
    struct timespec times[2] = {{seconds, 0}, {seconds, 0}};
    ASSERT_EQ(utimensat(AT_FDCWD, path.c_str(), times, 0), 0);
}

// The stamp a page would have recorded for the file as it is now: time and size from GIO, the
// hash of the bytes.
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

struct Banner {
    std::string text;
    std::string primary;
    std::string secondary;
    std::function<void()> on_primary;
    std::function<void()> on_secondary;
};

struct Reload {
    DocumentId id = 0;
    std::string message;
    ScanResult scan;
};

struct Ask {
    std::string message;
    std::string detail;
    std::function<void()> go;
};

// A host that records what the controller asks of it. A document has a page when the test puts a
// state for its id into `pages`.
class FakeHost : public ExternalChangeHost {
public:
    std::optional<PageState> page_state(DocumentId id) const override {
        auto found = pages.find(id);
        if (found == pages.end()) {
            return std::nullopt;
        }
        return found->second;
    }
    void set_baseline(DocumentId id, const FileStamp& stamp) override {
        baselines.push_back({id, stamp});
        auto found = pages.find(id);
        if (found != pages.end() && found->second.baseline) {
            found->second.baseline->stamp = stamp;
        }
    }
    void show_banner(DocumentId id, const std::string& text, const std::string& primary,
                     const std::string& secondary, std::function<void()> on_primary,
                     std::function<void()> on_secondary) override {
        banners[id] =
            Banner{text, primary, secondary, std::move(on_primary), std::move(on_secondary)};
        ++banners_shown;
    }
    void hide_banner(DocumentId id) override { banners.erase(id); }
    bool reload(DocumentId id, const ScanResult& scan, const std::string& text) override {
        if (!reload_ok) {
            ++reloads_refused;
            return false;
        }
        reloads.push_back({id, text, scan});
        return true;
    }
    void mark_modified(DocumentId id) override { marked_modified.push_back(id); }
    void deleted_on_disk(DocumentId id) override { deleted.push_back(id); }
    void ask_reload_anyway(const std::string& text, const std::string& detail,
                           std::function<void()> go) override {
        asks.push_back({text, detail, std::move(go)});
    }
    void message(const std::string& text) override { messages.push_back(text); }

    bool nothing_happened() const {
        return baselines.empty() && banners.empty() && reloads.empty() && marked_modified.empty() &&
               deleted.empty() && asks.empty() && messages.empty();
    }

    std::map<DocumentId, PageState> pages;
    std::map<DocumentId, Banner> banners;
    std::vector<std::pair<DocumentId, FileStamp>> baselines;
    std::vector<Reload> reloads;
    std::vector<DocumentId> marked_modified;
    std::vector<DocumentId> deleted;
    std::vector<Ask> asks;
    std::vector<std::string> messages;
    bool reload_ok = true;
    int banners_shown = 0;
    int reloads_refused = 0;
};

class ExternalChangeControllerTest : public ::testing::Test {
protected:
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

    // Looks at the document's file and waits for the answer (and a scan or a reload it starts).
    void check(DocumentId id) {
        controller_.check(id);
        ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    }

    std::string path_of(DocumentId id) const { return list_.find(id)->path; }

    TempDir dir_;
    DocumentList list_;
    FakeHost host_;
    ExternalChangeController controller_{list_, host_, std::make_unique<GlibTimer>(), 40};
};

TEST_F(ExternalChangeControllerTest, AnUnchangedFileDoesNothing) {
    DocumentId id = open("a.txt", "one\n");
    check(id);
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, ATouchedFileOnlyUpdatesTheStamp) {
    DocumentId id = open("a.txt", "one\n");
    const FileStamp before = host_.pages[id].baseline->stamp;
    set_time(path_of(id), 1500000000);
    check(id);
    ASSERT_EQ(host_.baselines.size(), 1u);
    EXPECT_EQ(host_.baselines[0].first, id);
    EXPECT_EQ(host_.baselines[0].second.mtime_us, 1500000000LL * 1000000);
    EXPECT_EQ(host_.baselines[0].second.size, before.size);
    EXPECT_EQ(host_.baselines[0].second.hash, before.hash);
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_TRUE(host_.messages.empty());
}

TEST_F(ExternalChangeControllerTest, AChangedCleanFileReloadsSilently) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    check(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].id, id);
    EXPECT_EQ(host_.reloads[0].message, vermilion::reloaded_message("a.txt"));
    EXPECT_EQ(host_.reloads[0].scan.size, 8u);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_TRUE(host_.messages.empty());
}

TEST_F(ExternalChangeControllerTest, AChangedEditedFileShowsTheBanner) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    write_file(path_of(id), "one\ntwo\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    const Banner& banner = host_.banners[id];
    EXPECT_EQ(banner.text, vermilion::banner_text(BannerKind::Changed, "a.txt"));
    EXPECT_EQ(banner.primary, "Reload");
    EXPECT_EQ(banner.secondary, "Keep mine");
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeControllerTest, ALargeCleanFileShowsTheBannerWithIgnore) {
    DocumentId id = open("big.txt", "one\n");
    fs::resize_file(path_of(id), vermilion::kLargeFileBytes + 1);
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::ChangedLarge, "big.txt"));
    EXPECT_EQ(host_.banners[id].secondary, "Ignore");
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeControllerTest, ADeletedFileIsMarkedAndAnnouncedOnce) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    check(id);
    EXPECT_TRUE(list_.find(id)->deleted_on_disk);
    ASSERT_EQ(host_.deleted.size(), 1u);
    EXPECT_EQ(host_.deleted[0], id);
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::deleted_message("a.txt"));
    check(id);
    EXPECT_EQ(host_.deleted.size(), 1u);
    EXPECT_EQ(host_.messages.size(), 1u);
}

TEST_F(ExternalChangeControllerTest, ADeletedEditedFileKeepsItsEditedFlag) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    fs::remove(path_of(id));
    check(id);
    EXPECT_TRUE(list_.find(id)->deleted_on_disk);
    EXPECT_TRUE(list_.find(id)->modified);
}

TEST_F(ExternalChangeControllerTest, AFileThatComesBackReloadsACleanDocument) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    check(id);
    write_file(path_of(id), "one\n");
    check(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].id, id);
    EXPECT_TRUE(host_.banners.empty());
}

TEST_F(ExternalChangeControllerTest, AFileThatComesBackShowsTheBannerForAnEditedDocument) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    fs::remove(path_of(id));
    check(id);
    write_file(path_of(id), "other\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::Returned, "a.txt"));
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeControllerTest, ADeletedAgainFileTakesTheReturnedBannerDown) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    fs::remove(path_of(id));
    check(id);
    write_file(path_of(id), "other\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    fs::remove(path_of(id));
    check(id);
    EXPECT_TRUE(host_.banners.empty());
}

TEST_F(ExternalChangeControllerTest, KeepMineAcceptsTheFileAsItIsNow) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    write_file(path_of(id), "one\ntwo\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    host_.banners[id].on_secondary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    ASSERT_EQ(host_.baselines.size(), 1u);
    const FileStamp now = stamp_of(path_of(id));
    EXPECT_EQ(host_.baselines[0].second.mtime_us, now.mtime_us);
    EXPECT_EQ(host_.baselines[0].second.size, now.size);
    EXPECT_EQ(host_.baselines[0].second.hash, now.hash);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_TRUE(host_.marked_modified.empty());
    EXPECT_TRUE(host_.reloads.empty());
    check(id);
    EXPECT_TRUE(host_.banners.empty()) << "the same file must not raise the banner again";
}

TEST_F(ExternalChangeControllerTest, IgnoreOnALargeFileAcceptsItToo) {
    DocumentId id = open("big.txt", "one\n");
    fs::resize_file(path_of(id), vermilion::kLargeFileBytes + 1);
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    host_.banners[id].on_secondary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_EQ(host_.baselines.size(), 1u);
    EXPECT_EQ(host_.baselines[0].second.size, vermilion::kLargeFileBytes + 1);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_TRUE(host_.reloads.empty());
}

TEST_F(ExternalChangeControllerTest, KeepMineOnAReturnedFileLeavesTheDocumentNeedingASave) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    check(id);
    ASSERT_TRUE(list_.find(id)->deleted_on_disk);
    write_file(path_of(id), std::string("a\0b\n", 4)); // a NUL: the silent reload is refused
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::Returned, "a.txt"));
    host_.banners[id].on_secondary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    ASSERT_EQ(host_.marked_modified.size(), 1u);
    EXPECT_EQ(host_.marked_modified[0], id);
    EXPECT_EQ(host_.baselines.size(), 1u);
    EXPECT_TRUE(host_.banners.empty());
}

TEST_F(ExternalChangeControllerTest, KeepMineOnAReturnedEditedDocumentDoesNotMarkItAgain) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    fs::remove(path_of(id));
    check(id);
    write_file(path_of(id), "other\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    host_.banners[id].on_secondary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    EXPECT_TRUE(list_.find(id)->modified);
    EXPECT_TRUE(host_.marked_modified.empty());
}

TEST_F(ExternalChangeControllerTest, TheBannersReloadScansTheFileAndReloads) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    write_file(path_of(id), "one\ntwo\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    host_.banners[id].on_primary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].id, id);
    EXPECT_EQ(host_.reloads[0].scan.size, 8u);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_TRUE(host_.asks.empty());
}

TEST_F(ExternalChangeControllerTest, AFileWithANulIsNotReloadedSilently) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string("a\0b\n", 4));
    check(id);
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.asks.empty());
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::Changed, "a.txt"));
}

TEST_F(ExternalChangeControllerTest, TheBannersReloadAsksBeforeReloadingAFileWithANul) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string("a\0b\n", 4));
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    host_.banners[id].on_primary();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    ASSERT_EQ(host_.asks.size(), 1u);
    const vermilion::OpenQuestion question =
        vermilion::open_question("a.txt", OpenWarning::Binary, 0);
    EXPECT_EQ(host_.asks[0].message, question.message);
    EXPECT_EQ(host_.asks[0].detail, question.detail);
    EXPECT_TRUE(host_.reloads.empty());
    host_.asks[0].go();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_TRUE(host_.reloads[0].scan.binary);
}

TEST_F(ExternalChangeControllerTest, ALineLongerThanTheLimitIsNotReloadedSilently) {
    controller_.set_long_line_limit(10000);
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string(20000, 'a') + "\n");
    check(id);
    EXPECT_TRUE(host_.reloads.empty());
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::Changed, "a.txt"));
}

TEST_F(ExternalChangeControllerTest, ALineWithinTheLimitIsReloadedSilently) {
    controller_.set_long_line_limit(30000);
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string(20000, 'a') + "\n");
    check(id);
    EXPECT_TRUE(host_.banners.empty());
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].scan.longest_line, 20000u);
}

TEST_F(ExternalChangeControllerTest, TheDefaultLimitStaysUntilItIsChanged) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), std::string(20000, 'a') + "\n");
    check(id);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeControllerTest, AnOtherEncodingIsAnnounced) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "caf\xE9\n"); // not UTF-8
    check(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].message,
              vermilion::reloaded_encoding_message("a.txt", "Windows-1252"));
}

TEST_F(ExternalChangeControllerTest, AChosenEncodingIsNotAnnounced) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_encoding_chosen(id, true);
    write_file(path_of(id), "caf\xE9\n");
    check(id);
    ASSERT_EQ(host_.reloads.size(), 1u);
    EXPECT_EQ(host_.reloads[0].message, vermilion::reloaded_message("a.txt"));
}

TEST_F(ExternalChangeControllerTest, AnUntitledOrUnknownDocumentIsLeftAlone) {
    DocumentId untitled = list_.add_untitled();
    host_.pages[untitled].quiet = true;
    controller_.check(untitled);
    controller_.check(9999);
    EXPECT_FALSE(controller_.busy());
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, APageWithoutABaselineIsLeftAlone) {
    DocumentId id = open("a.txt", "one\n");
    host_.pages[id].baseline.reset();
    write_file(path_of(id), "two\n");
    controller_.check(id);
    EXPECT_FALSE(controller_.busy());
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, ABusyPageWaitsForSettled) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    host_.pages[id].quiet = false;
    controller_.check(id);
    EXPECT_FALSE(controller_.busy());
    EXPECT_TRUE(host_.nothing_happened());
    controller_.settled(id);
    EXPECT_FALSE(controller_.busy()) << "still not quiet: nothing starts";
    host_.pages[id].quiet = true;
    controller_.settled(id);
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeControllerTest, SettledWithNothingWaitingDoesNothing) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "two\n");
    controller_.settled(id);
    EXPECT_FALSE(controller_.busy());
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, APageThatGoesAwayDuringACheckDropsTheAnswer) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    controller_.check(id);
    host_.pages.erase(id);
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, ABaselineThatCaughtUpDuringTheCheckLeavesNothingToDo) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    controller_.check(id);
    // The page took the new file as its baseline while the check was reading.
    host_.pages[id].baseline->stamp = stamp_of(path_of(id));
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_TRUE(host_.reloads.empty());
    EXPECT_TRUE(host_.banners.empty());
}

TEST_F(ExternalChangeControllerTest, ARefusedReloadIsTriedAgainWhenTheDocumentSettles) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    host_.reload_ok = false;
    check(id);
    EXPECT_EQ(host_.reloads_refused, 1);
    EXPECT_TRUE(host_.reloads.empty());
    host_.reload_ok = true;
    controller_.settled(id);
    ASSERT_TRUE(pump_until([this] { return !controller_.busy(); }));
    EXPECT_EQ(host_.reloads.size(), 1u);
}

TEST_F(ExternalChangeControllerTest, AFileThatIsTheSameAgainTakesTheBannerDown) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    write_file(path_of(id), "one\ntwo\n");
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    write_file(path_of(id), "one\n");
    set_time(path_of(id), 1500000000);
    check(id);
    EXPECT_TRUE(host_.banners.empty());
    EXPECT_EQ(host_.baselines.size(), 1u);
}

TEST_F(ExternalChangeControllerTest, ATimeOnlyBaselineWithTheSameTimeLearnsSizeAndHash) {
    DocumentId id = open("a.txt", "one\n");
    FileStamp stamp;
    stamp.mtime_us = host_.pages[id].baseline->stamp.mtime_us;
    stamp.time_only = true;
    host_.pages[id].baseline->stamp = stamp;
    check(id);
    ASSERT_EQ(host_.baselines.size(), 1u);
    EXPECT_FALSE(host_.baselines[0].second.time_only);
    EXPECT_EQ(host_.baselines[0].second.size, 4u);
    EXPECT_TRUE(host_.baselines[0].second.hash.has_value());
    EXPECT_TRUE(host_.banners.empty());
}

TEST_F(ExternalChangeControllerTest, ATimeOnlyBaselineWithAnotherTimeIsAChange) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    FileStamp stamp;
    stamp.mtime_us = 1500000000LL * 1000000;
    stamp.time_only = true;
    host_.pages[id].baseline->stamp = stamp;
    check(id);
    ASSERT_EQ(host_.banners.count(id), 1u);
    EXPECT_EQ(host_.banners[id].text, vermilion::banner_text(BannerKind::Changed, "a.txt"));
}

TEST_F(ExternalChangeControllerTest, FollowActiveWatchesTheActiveFileOnly) {
    DocumentId a = open("a.txt", "one\n");
    EXPECT_EQ(list_.active(), a);
    controller_.follow_active();
    EXPECT_EQ(controller_.watched_path(), path_of(a));
    pump_until([this] { return !controller_.busy(); });
    DocumentId b = open("b.txt", "two\n");
    EXPECT_EQ(list_.active(), b);
    controller_.follow_active();
    EXPECT_EQ(controller_.watched_path(), path_of(b));
    pump_until([this] { return !controller_.busy(); });
    list_.add_untitled();
    controller_.follow_active();
    EXPECT_TRUE(controller_.watched_path().empty());
    EXPECT_TRUE(host_.nothing_happened());
}

TEST_F(ExternalChangeControllerTest, SwitchingToATabChecksItsFile) {
    DocumentId a = open("a.txt", "one\n");
    DocumentId b = open("b.txt", "two\n");
    controller_.follow_active();
    pump_until([this] { return !controller_.busy(); });
    write_file(path_of(a), "one\nchanged\n"); // a is not watched: nothing sees it yet
    pump_for(150);
    EXPECT_TRUE(host_.reloads.empty());
    list_.set_active(a);
    controller_.follow_active();
    ASSERT_TRUE(pump_until([this] { return !controller_.busy() && !host_.reloads.empty(); }));
    EXPECT_EQ(host_.reloads[0].id, a);
    (void)b;
}

TEST_F(ExternalChangeControllerTest, AChangeOnDiskReachesTheActiveDocumentThroughTheMonitor) {
    DocumentId id = open("a.txt", "one\n");
    controller_.follow_active();
    pump_until([this] { return !controller_.busy(); });
    write_file(path_of(id), "one\ntwo\n");
    ASSERT_TRUE(pump_until([this] { return !host_.reloads.empty(); }));
    EXPECT_EQ(host_.reloads[0].id, id);
}

TEST_F(ExternalChangeControllerTest, ADeleteReachesTheActiveDocumentThroughTheMonitor) {
    DocumentId id = open("a.txt", "one\n");
    controller_.follow_active();
    pump_until([this] { return !controller_.busy(); });
    fs::remove(path_of(id));
    ASSERT_TRUE(pump_until([this] { return !host_.deleted.empty(); }));
    EXPECT_TRUE(list_.find(id)->deleted_on_disk);
}

TEST_F(ExternalChangeControllerTest, ADestroyedControllerAnswersNothing) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    {
        ExternalChangeController other(list_, host_, std::make_unique<GlibTimer>(), 40);
        other.check(id);
    }
    pump_for(200);
    EXPECT_TRUE(host_.nothing_happened());
}

} // namespace
