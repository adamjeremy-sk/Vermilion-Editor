// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery controller: its timer, worker thread and files, on a real main loop.

#include <gtest/gtest.h>

#include <glib.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_schedule.h"
#include "core/recovery_store.h"
#include "ui/recovery_controller.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::TextFormat;
using vermilion::ui::RecoveryController;

constexpr const char* kTooLarge = "Too large for recovery copies";
constexpr const char* kFailed = "Could not write a recovery copy.";
constexpr const char* kWorkingAgain = "Recovery copies are working again.";
constexpr std::int64_t kFileTimeUs = 1700000000123456;
constexpr std::size_t kNotRead = 9999; // a count that could not be read

// Runs the main loop for a while: the timer fires and the worker's answers come back meanwhile.
void run_for(int ms) {
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    g_timeout_add_once(
        static_cast<guint>(ms),
        [](gpointer data) { g_main_loop_quit(static_cast<GMainLoop*>(data)); }, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

// Runs the loop until done() holds or the limit passes; true when done() holds.
template <typename Done>
bool run_until(Done done, int limit_ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(limit_ms) * 1000;
    while (!done() && g_get_monotonic_time() < end) {
        run_for(20);
    }
    return done();
}

std::size_t file_count(const std::string& root) {
    vermilion::Result<std::vector<std::string>> ids =
        RecoveryStore(root, RecoveryProgram::Light).list();
    return ids.is_ok() ? ids.value().size() : kNotRead;
}

// A document list with a controller on it; the text of each document is set by the test.
struct Session {
    explicit Session(const std::string& root)
        : controller(std::make_unique<RecoveryController>(
              list, root, RecoveryProgram::Light,
              RecoveryController::Callbacks{
                  [this](DocumentId id) -> std::optional<std::string> {
                      ++takes;
                      auto found = texts.find(id);
                      if (found == texts.end()) {
                          return std::nullopt;
                      }
                      return found->second;
                  },
                  [this](const std::string& message) { messages.push_back(message); }})) {}

    DocumentId add_untitled(const std::string& text) {
        DocumentId id = list.add_untitled();
        list.set_modified(id, true);
        texts[id] = text;
        return id;
    }

    DocumentId add_file(const std::string& path, const std::string& text) {
        DocumentId id = list.add_file(path, "file", TextFormat{}, false);
        list.set_modified(id, true);
        texts[id] = text;
        return id;
    }

    DocumentList list;
    std::map<DocumentId, std::string> texts;
    std::vector<std::string> messages;
    int takes = 0;
    std::unique_ptr<RecoveryController> controller; // last: it goes first
};

struct Found {
    std::string text;
    std::optional<std::string> uri;
    std::optional<std::string> title;
    std::optional<std::int64_t> file_time_us;
    std::uint64_t length = 0;
};

// What the timeline saw. Every test reads one part of it.
struct Observed {
    bool ready = false;
    bool leftover_gone = false;
    int takes_before_idle = -1;
    std::vector<std::string> messages_before_idle;
    std::size_t files_after_idle = kNotRead;
    int takes_after_idle = -1;
    std::size_t files_at_end = kNotRead;
    int takes_at_end = -1;
    std::vector<Found> found;
    std::vector<std::string> messages_at_end;
    std::vector<std::string> bad_messages;
    std::vector<std::string> bad_messages_final;
    std::size_t bad_files_final = kNotRead;
    std::size_t files_after_ends = kNotRead;
    std::size_t files_after_destroy = kNotRead;
    bool large_taken = false;
    std::size_t large_files_after_destroy = kNotRead;
    std::vector<std::string> large_messages;
    int closed_takes = -1;
};

std::vector<Found> read_all(const std::string& root) {
    std::vector<Found> found;
    RecoveryStore store(root, RecoveryProgram::Light);
    vermilion::Result<std::vector<std::string>> ids = store.list();
    if (!ids.is_ok()) {
        return found;
    }
    for (const std::string& id : ids.value()) {
        auto copy = store.read(id);
        if (copy.is_ok()) {
            const vermilion::RecoveryHeader& header = copy.value().header;
            found.push_back(Found{copy.value().text, header.uri, header.title, header.file_mtime_us,
                                  header.length});
        }
    }
    return found;
}

// One run of about eleven seconds on the real clock (the schedule waits five seconds of quiet
// before it copies). Four sessions share the loop: the main one, one whose folder cannot be made,
// one with a very large document whose controller ends while its copy may still be written, and one
// whose only document is closed before its copy.
Observed run_timeline() {
    namespace fs = std::filesystem;
    Observed seen;
    g_autofree char* made = g_dir_make_tmp("vermilion-controller-XXXXXX", nullptr);
    if (made == nullptr) {
        return seen;
    }
    const std::string base = made;
    const std::string main_root = base + "/main";
    const std::string blocker = base + "/blocker";
    g_file_set_contents(blocker.c_str(), "x", 1, nullptr);
    RecoveryStore main_store(main_root, RecoveryProgram::Light);
    g_mkdir_with_parents(main_store.folder().c_str(), 0700);
    const std::string leftover = main_store.folder() + "/.vrec-tmp-abcdef";
    g_file_set_contents(leftover.c_str(), "junk", 4, nullptr);

    auto session = std::make_unique<Session>(main_root);
    seen.leftover_gone = !g_file_test(leftover.c_str(), G_FILE_TEST_EXISTS);
    Session bad(blocker + "/sub"); // a folder under a regular file cannot be made
    Session closed(base + "/closed");
    const std::string large_root = base + "/large";
    Session large(large_root);

    DocumentId a = session->add_untitled("alpha");
    DocumentId b = session->add_untitled("bravo");
    DocumentId h = session->add_untitled("hotel");
    DocumentId i = session->add_untitled("india");
    DocumentId c = session->add_file("/tmp/x y.txt", "charlie v1");
    DocumentId d = session->add_untitled("delta");
    DocumentId f = bad.add_untitled("foxtrot");
    DocumentId f2 = bad.add_untitled("golf");
    DocumentId x = closed.add_untitled("x");
    const std::string big(40 * 1024 * 1024, 'q');
    DocumentId g = large.add_untitled(big);

    session->controller->document_synced(c, kFileTimeUs);
    for (DocumentId id : {a, b, h, i}) {
        session->controller->document_edited(id, 5);
    }
    session->controller->document_edited(c, 10);
    session->controller->document_edited(d, vermilion::kRecoveryCeilingChars + 1);
    bad.controller->document_edited(f, 7);
    bad.controller->document_edited(f2, 4);
    closed.controller->document_edited(x, 1);
    closed.controller->document_closing(x);
    closed.list.close(x);
    large.controller->document_edited(g, big.size());

    run_for(1500);
    session->texts[c] = "charlie v2";
    session->controller->document_edited(c, 10);
    run_for(1500);
    session->texts[c] = "charlie v3";
    session->controller->document_edited(c, 10);
    seen.takes_before_idle = session->takes; // 3.0 s in
    seen.messages_before_idle = session->messages;

    // The large document is copied at about 5 s; its session ends at once, with the write running.
    seen.large_taken = run_until([&] { return large.takes == 1; }, 9000);
    large.controller.reset();
    seen.large_files_after_destroy = file_count(large_root);
    run_for(300); // answers of the dead controller are dropped, not spoken
    seen.large_messages = large.messages;

    run_for(1000);
    seen.files_after_idle = file_count(main_root); // about 6.3 s in: a, b, h, i, not yet c
    seen.takes_after_idle = session->takes;
    std::error_code ignored;
    fs::remove(blocker, ignored); // from now on the folder of the failing session can be made
    run_for(2900);
    seen.files_at_end = file_count(main_root); // about 9.2 s in: c is there too
    seen.takes_at_end = session->takes;
    seen.found = read_all(main_root);
    seen.messages_at_end = session->messages;
    seen.bad_messages = bad.messages;
    seen.closed_takes = closed.takes;

    session->controller->document_modified(a, false);
    session->controller->document_saved(h);
    session->controller->document_reloading(i);
    session->controller->document_modified(d, true); // the flag turning on says nothing new
    session->controller->document_closing(b);
    session->list.close(b);
    // The failing session tries again at about 10 s.
    run_for(1800);
    seen.files_after_ends = file_count(main_root); // only c is left
    seen.bad_messages_final = bad.messages;
    seen.bad_files_final = file_count(blocker + "/sub");
    session.reset();
    seen.files_after_destroy = file_count(main_root);
    bad.controller.reset();
    fs::remove_all(base, ignored);
    seen.ready = true;
    return seen;
}

class RecoveryControllerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { seen() = run_timeline(); }
    static Observed& seen() {
        static Observed observed;
        return observed;
    }
};

TEST_F(RecoveryControllerTest, DeletesLeftoverTemporaryFilesAtStart) {
    ASSERT_TRUE(seen().ready);
    EXPECT_TRUE(seen().leftover_gone);
}

TEST_F(RecoveryControllerTest, CopiesNothingBeforeTheIdleTime) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().takes_before_idle, 0);
}

TEST_F(RecoveryControllerTest, AnnouncesATooLargeDocumentOnceAndNeverCopiesIt) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().messages_before_idle, std::vector<std::string>{kTooLarge});
    EXPECT_EQ(seen().messages_at_end, std::vector<std::string>{kTooLarge});
    EXPECT_EQ(seen().takes_at_end, 5); // a, b, h, i and c; not d
}

TEST_F(RecoveryControllerTest, WritesTheCopiesThatAreDueAfterTheIdleTime) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().files_after_idle, 4u);
    EXPECT_EQ(seen().takes_after_idle, 4);
}

TEST_F(RecoveryControllerTest, JoinsSeveralEditsIntoOneCopyWithTheLastText) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().files_at_end, 5u);
    int charlie = 0;
    for (const Found& copy : seen().found) {
        if (copy.text == "charlie v3") {
            ++charlie;
        }
        EXPECT_NE(copy.text, "charlie v1");
        EXPECT_NE(copy.text, "charlie v2");
    }
    EXPECT_EQ(charlie, 1);
}

TEST_F(RecoveryControllerTest, RecordsTheFileAndItsTimeInTheCopyOfAFileDocument) {
    ASSERT_TRUE(seen().ready);
    int with_file = 0;
    for (const Found& copy : seen().found) {
        if (copy.uri) {
            ++with_file;
            EXPECT_EQ(*copy.uri, "file:///tmp/x%20y.txt");
            EXPECT_EQ(copy.file_time_us, std::optional<std::int64_t>(kFileTimeUs));
            EXPECT_EQ(copy.length, 10u);
        }
    }
    EXPECT_EQ(with_file, 1);
}

TEST_F(RecoveryControllerTest, RecordsATitleAndNoFileTimeInTheCopyOfAnUntitledDocument) {
    ASSERT_TRUE(seen().ready);
    int untitled = 0;
    for (const Found& copy : seen().found) {
        if (!copy.uri) {
            ++untitled;
            EXPECT_TRUE(copy.title.has_value());
            EXPECT_FALSE(copy.file_time_us.has_value());
            EXPECT_EQ(copy.length, 5u);
        }
    }
    EXPECT_EQ(untitled, 4);
}

TEST_F(RecoveryControllerTest, AnnouncesAFailedWriteOnce) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().bad_messages, std::vector<std::string>{kFailed});
}

TEST_F(RecoveryControllerTest, AnnouncesOnceWhenCopiesWorkAgain) {
    ASSERT_TRUE(seen().ready);
    const std::vector<std::string> expected{kFailed, kWorkingAgain};
    EXPECT_EQ(seen().bad_messages_final, expected); // two copies succeed, one message
}

TEST_F(RecoveryControllerTest, WritesTheCopiesOnceTheFolderCanBeMade) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().bad_files_final, 2u);
}

TEST_F(RecoveryControllerTest, DeletesTheCopyWhenTheDocumentIsSavedReloadedOrClosed) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().files_after_ends, 1u); // a (flag off), h (saved), i (reloaded), b (closed)
}

TEST_F(RecoveryControllerTest, DeletesTheRemainingCopiesWhenItEnds) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().files_after_destroy, 0u);
}

TEST_F(RecoveryControllerTest, LeavesNoFileWhenItEndsWhileALargeCopyIsWritten) {
    ASSERT_TRUE(seen().ready);
    EXPECT_TRUE(seen().large_taken);
    EXPECT_EQ(seen().large_files_after_destroy, 0u);
}

TEST_F(RecoveryControllerTest, SaysNothingAfterItHasEnded) {
    ASSERT_TRUE(seen().ready);
    EXPECT_TRUE(seen().large_messages.empty());
}

TEST_F(RecoveryControllerTest, NeverCopiesADocumentClosedBeforeItsCopy) {
    ASSERT_TRUE(seen().ready);
    EXPECT_EQ(seen().closed_takes, 0);
}

} // namespace
