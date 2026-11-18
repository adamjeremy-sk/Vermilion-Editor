// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for ExternalChangeController::look(): real files and inspections, a recording fake host.

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
#include <vector>

#include "core/document_list.h"
#include "core/external_change.h"
#include "core/external_messages.h"
#include "core/file_hash.h"
#include "core/text_format.h"
#include "core/text_scan.h"
#include "ui/external_change_controller.h"
#include "ui/glib_timer.h"

namespace {

namespace fs = std::filesystem;

using vermilion::Baseline;
using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::FileLook;
using vermilion::FileStamp;
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
        char pattern[] = "/tmp/vermilion-look-XXXXXX";
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

void set_time(const std::string& path, long seconds) {
    struct timespec times[2] = {{seconds, 0}, {seconds, 0}};
    ASSERT_EQ(utimensat(AT_FDCWD, path.c_str(), times, 0), 0);
}

// The stamp a page would have recorded for the file as it is now.
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

// A host that records every call except page_state(). A look must call only hide_banner(),
// deleted_on_disk() and message(), and those only for a file found missing.
class RecordingHost : public ExternalChangeHost {
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
    void hide_banner(DocumentId id) override { hidden.push_back(id); }
    bool reload(DocumentId, const ScanResult&, const std::string&) override {
        ++other_calls;
        return true;
    }
    void mark_modified(DocumentId) override { ++other_calls; }
    void deleted_on_disk(DocumentId id) override { deleted.push_back(id); }
    void ask_reload_anyway(const std::string&, const std::string&, std::function<void()>) override {
        ++other_calls;
    }
    void message(const std::string& text) override { messages.push_back(text); }

    bool untouched() const {
        return other_calls == 0 && hidden.empty() && deleted.empty() && messages.empty();
    }

    std::map<DocumentId, PageState> pages;
    std::vector<DocumentId> hidden;
    std::vector<DocumentId> deleted;
    std::vector<std::string> messages;
    int other_calls = 0;
};

class ExternalChangeLookTest : public ::testing::Test {
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

    // Looks and waits for the answer; the answer must come exactly once.
    FileLook look(DocumentId id) {
        int answers = 0;
        std::optional<FileLook> answer;
        controller_.look(id, [&](FileLook result) {
            ++answers;
            answer = result;
        });
        EXPECT_TRUE(pump_until([&] { return answer.has_value(); }));
        pump_for(30);
        EXPECT_EQ(answers, 1);
        return answer.value_or(FileLook::Unknown);
    }

    std::string path_of(DocumentId id) const { return list_.find(id)->path; }

    TempDir dir_;
    DocumentList list_;
    RecordingHost host_;
    ExternalChangeController controller_{list_, host_, std::make_unique<GlibTimer>(), 40};
};

TEST_F(ExternalChangeLookTest, AnUnchangedFileIsTheSame) {
    DocumentId id = open("a.txt", "one\n");
    EXPECT_EQ(look(id), FileLook::Same);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, ATouchedFileIsTheSameAndTheBaselineIsLeftAlone) {
    DocumentId id = open("a.txt", "one\n");
    set_time(path_of(id), 1500000000);
    EXPECT_EQ(look(id), FileLook::Same);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AChangedCleanFileIsChangedAndNothingIsReloaded) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    EXPECT_EQ(look(id), FileLook::Changed);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AChangedEditedFileIsChangedAndNoBannerIsShown) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_modified(id, true);
    write_file(path_of(id), "one\ntwo\n");
    EXPECT_EQ(look(id), FileLook::Changed);
    EXPECT_TRUE(host_.untouched());
    EXPECT_TRUE(list_.find(id)->modified);
}

TEST_F(ExternalChangeLookTest, ALargeChangedFileIsChanged) {
    DocumentId id = open("big.txt", "one\n");
    fs::resize_file(path_of(id), vermilion::kLargeFileBytes + 1);
    EXPECT_EQ(look(id), FileLook::Changed);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AMissingFileIsGoneAndMarkedOnce) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    EXPECT_EQ(look(id), FileLook::Gone);
    EXPECT_TRUE(list_.find(id)->deleted_on_disk);
    EXPECT_EQ(host_.deleted, std::vector<DocumentId>{id});
    EXPECT_EQ(host_.hidden, std::vector<DocumentId>{id});
    ASSERT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.messages[0], vermilion::deleted_message("a.txt"));
    EXPECT_EQ(look(id), FileLook::Gone);
    EXPECT_EQ(host_.deleted.size(), 1u);
    EXPECT_EQ(host_.hidden.size(), 1u);
    EXPECT_EQ(host_.messages.size(), 1u);
    EXPECT_EQ(host_.other_calls, 0);
}

TEST_F(ExternalChangeLookTest, AFileMarkedDeletedAndStillMissingIsGoneWithNoNewCalls) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_deleted_on_disk(id, true);
    fs::remove(path_of(id));
    EXPECT_EQ(look(id), FileLook::Gone);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AFileMarkedDeletedThatCameBackIsChanged) {
    DocumentId id = open("a.txt", "one\n");
    list_.set_deleted_on_disk(id, true);
    EXPECT_EQ(look(id), FileLook::Changed);
    EXPECT_TRUE(list_.find(id)->deleted_on_disk);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, ABaselineFromACopyIsComparedByTime) {
    DocumentId id = open("a.txt", "one\n");
    FileStamp copy;
    copy.mtime_us = host_.pages[id].baseline->stamp.mtime_us;
    copy.time_only = true;
    host_.pages[id].baseline->stamp = copy;
    EXPECT_EQ(look(id), FileLook::Same);
    host_.pages[id].baseline->stamp.mtime_us -= 5000000;
    EXPECT_EQ(look(id), FileLook::Changed);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AnUntitledOrUnknownDocumentIsUnknown) {
    DocumentId untitled = list_.add_untitled();
    EXPECT_EQ(look(untitled), FileLook::Unknown);
    EXPECT_EQ(look(9999), FileLook::Unknown);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, APageThatIsNotQuietOrHasNoBaselineIsUnknown) {
    DocumentId busy = open("a.txt", "one\n");
    host_.pages[busy].quiet = false;
    EXPECT_EQ(look(busy), FileLook::Unknown);
    DocumentId bare = open("b.txt", "two\n");
    host_.pages[bare].baseline.reset();
    EXPECT_EQ(look(bare), FileLook::Unknown);
    DocumentId moved = open("c.txt", "three\n");
    host_.pages[moved].baseline->path = dir_.path("elsewhere.txt");
    EXPECT_EQ(look(moved), FileLook::Unknown);
    DocumentId pageless = open("d.txt", "four\n");
    host_.pages.erase(pageless);
    EXPECT_EQ(look(pageless), FileLook::Unknown);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AChangedFileThatCannotBeReadIsUnknown) {
    if (geteuid() == 0) {
        return; // a superuser reads any file, so the case cannot be made
    }
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    ASSERT_EQ(chmod(path_of(id).c_str(), 0), 0);
    EXPECT_EQ(look(id), FileLook::Unknown);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AFileTheSystemCannotLookAtIsUnknownNotGone) {
    write_file(dir_.path("plain"), "not a folder\n"); // so "plain/a.txt" runs through a file
    const std::string path = dir_.path("plain/a.txt");
    DocumentId id = list_.add_file(path, "a.txt", TextFormat{}, false);
    ExternalChangeHost::PageState state;
    state.quiet = true;
    state.baseline = Baseline{path, stamp_of(dir_.path("plain"))};
    host_.pages[id] = state;
    EXPECT_EQ(look(id), FileLook::Unknown);
    EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AMissingFileIsNotMarkedWhileThePageIsBusy) {
    DocumentId id = open("a.txt", "one\n");
    host_.pages[id].quiet = false;
    fs::remove(path_of(id));
    EXPECT_EQ(look(id), FileLook::Unknown);
    EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, ABaselineThatChangesDuringTheLookGivesUnknown) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    std::optional<FileLook> answer;
    controller_.look(id, [&](FileLook result) { answer = result; });
    host_.pages[id].baseline->stamp.size += 1;
    ASSERT_TRUE(pump_until([&] { return answer.has_value(); }));
    EXPECT_EQ(*answer, FileLook::Unknown);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, AnythingThatChangesDuringTheLookGivesUnknownAndNoMark) {
    // Each case: a document whose file is missing, a look, then a change of the document or its
    // page before the answer is used.
    const std::vector<std::function<void(DocumentId)>> changes = {
        [&](DocumentId id) { host_.pages[id].quiet = false; },
        [&](DocumentId id) { host_.pages.erase(id); },
        [&](DocumentId id) { host_.pages[id].baseline.reset(); },
        [&](DocumentId id) { host_.pages[id].baseline->path = dir_.path("elsewhere.txt"); },
        [&](DocumentId id) { list_.set_path(id, dir_.path("renamed.txt"), "renamed.txt"); },
    };
    int number = 0;
    for (const auto& change : changes) {
        DocumentId id = open("case" + std::to_string(++number) + ".txt", "one\n");
        fs::remove(path_of(id));
        std::optional<FileLook> answer;
        controller_.look(id, [&](FileLook result) { answer = result; });
        change(id);
        ASSERT_TRUE(pump_until([&] { return answer.has_value(); }));
        EXPECT_EQ(*answer, FileLook::Unknown);
        EXPECT_FALSE(list_.find(id)->deleted_on_disk);
    }
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, ADocumentClosedDuringTheLookGivesUnknownAndNoMark) {
    DocumentId id = open("a.txt", "one\n");
    fs::remove(path_of(id));
    std::optional<FileLook> answer;
    controller_.look(id, [&](FileLook result) { answer = result; });
    list_.close(id);
    host_.pages.erase(id);
    ASSERT_TRUE(pump_until([&] { return answer.has_value(); }));
    EXPECT_EQ(*answer, FileLook::Unknown);
    EXPECT_TRUE(host_.untouched());
}

TEST_F(ExternalChangeLookTest, SeveralLooksAtOnceAnswerOnceEach) {
    DocumentId first = open("a.txt", "one\n");
    DocumentId second = open("b.txt", "two\n");
    write_file(path_of(second), "two\nthree\n");
    std::vector<FileLook> answers(3, FileLook::Unknown);
    int count = 0;
    controller_.look(first, [&](FileLook result) { answers[0] = result, ++count; });
    controller_.look(second, [&](FileLook result) { answers[1] = result, ++count; });
    controller_.look(first, [&](FileLook result) { answers[2] = result, ++count; });
    ASSERT_TRUE(pump_until([&] { return count == 3; }));
    pump_for(30);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(answers[0], FileLook::Same);
    EXPECT_EQ(answers[1], FileLook::Changed);
    EXPECT_EQ(answers[2], FileLook::Same);
}

TEST_F(ExternalChangeLookTest, ALookDoesNotDisturbACheckOfTheSameDocument) {
    DocumentId id = open("a.txt", "one\n");
    write_file(path_of(id), "one\ntwo\n");
    controller_.check(id);
    std::optional<FileLook> answer;
    controller_.look(id, [&](FileLook result) { answer = result; });
    ASSERT_TRUE(pump_until([&] { return answer.has_value() && !controller_.busy(); }));
    EXPECT_EQ(*answer, FileLook::Changed);
}

TEST_F(ExternalChangeLookTest, ADestroyedControllerNeverAnswers) {
    DocumentId id = open("a.txt", "one\n");
    bool answered = false;
    {
        ExternalChangeController gone(list_, host_, std::make_unique<GlibTimer>(), 40);
        gone.look(id, [&](FileLook) { answered = true; });
    }
    pump_for(150);
    EXPECT_FALSE(answered);
}

} // namespace
