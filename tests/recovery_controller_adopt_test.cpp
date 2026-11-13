// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the controller taking over the id of a recovered document's copy, on a real main loop.

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
#include "core/recovery_file.h"
#include "core/recovery_store.h"
#include "ui/recovery_controller.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::RecoveryHeader;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::ui::RecoveryController;

constexpr const char* kCopy = "aaaaaaaa-0000-4000-8000-000000000001";
constexpr const char* kOther = "bbbbbbbb-0000-4000-8000-000000000002";
constexpr std::int64_t kFileTimeUs = 1700000000123456;

// A temporary directory as the recovery root, removed with the object.
struct Folder {
    Folder() {
        g_autofree char* made = g_dir_make_tmp("vermilion-adopt-XXXXXX", nullptr);
        if (made != nullptr) {
            root = made;
        }
    }
    ~Folder() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    Folder(const Folder&) = delete;
    Folder& operator=(const Folder&) = delete;

    std::string root;
};

RecoveryStore store_of(const Folder& folder) {
    return RecoveryStore(folder.root, RecoveryProgram::Light);
}

void put_copy(const Folder& folder, const std::string& id, const std::string& text) {
    RecoveryHeader header;
    header.title = "Untitled 1";
    header.copied_us = 1794452733554000;
    EXPECT_TRUE(store_of(folder).write(id, header, text).is_ok());
}

bool copy_exists(const Folder& folder, const std::string& id) {
    return g_file_test(store_of(folder).path_of(id).c_str(), G_FILE_TEST_EXISTS);
}

std::size_t file_count(const Folder& folder) {
    auto ids = store_of(folder).list();
    return ids.is_ok() ? ids.value().size() : 9999;
}

// Runs the main loop until done() holds or the limit passes; true when done() holds.
template <typename Done>
bool run_until(Done done, int limit_ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(limit_ms) * 1000;
    while (!done() && g_get_monotonic_time() < end) {
        GMainContext* context = g_main_context_default();
        g_main_context_iteration(context, FALSE);
        g_usleep(5000);
    }
    return done();
}

// A document list with a controller on it; the text of each document is set by the test.
struct Session {
    explicit Session(const std::string& root)
        : controller(std::make_unique<RecoveryController>(
              list, root, RecoveryProgram::Light,
              RecoveryController::Callbacks{[this](DocumentId id) -> std::optional<std::string> {
                                                auto found = texts.find(id);
                                                if (found == texts.end()) {
                                                    return std::nullopt;
                                                }
                                                return found->second;
                                            },
                                            [](const std::string&) {}})) {}

    DocumentId add_untitled(const std::string& text) {
        DocumentId id = list.add_untitled();
        list.set_modified(id, true);
        texts[id] = text;
        return id;
    }

    DocumentList list;
    std::map<DocumentId, std::string> texts;
    std::unique_ptr<RecoveryController> controller; // last: it goes first
};

TEST(RecoveryAdopt, TakesTheCopyOverAndDeletesItWhenTheDocumentIsSaved) {
    Folder folder;
    put_copy(folder, kCopy, "old text");
    Session session(folder.root);
    DocumentId id = session.add_untitled("old text");
    EXPECT_TRUE(session.controller->adopt(id, kCopy, kFileTimeUs));
    EXPECT_TRUE(copy_exists(folder, kCopy));
    session.controller->document_saved(id);
    EXPECT_TRUE(run_until([&] { return !copy_exists(folder, kCopy); }, 3000));
}

TEST(RecoveryAdopt, DeletesTheCopyWhenTheDocumentIsClosedWithoutSaving) {
    Folder folder;
    put_copy(folder, kCopy, "old text");
    Session session(folder.root);
    DocumentId id = session.add_untitled("old text");
    EXPECT_TRUE(session.controller->adopt(id, kCopy, std::nullopt));
    session.controller->document_closing(id);
    session.list.close(id);
    EXPECT_TRUE(run_until([&] { return !copy_exists(folder, kCopy); }, 3000));
}

TEST(RecoveryAdopt, DeletesTheCopyAtACleanEnd) {
    Folder folder;
    put_copy(folder, kCopy, "old text");
    Session session(folder.root);
    DocumentId id = session.add_untitled("old text");
    EXPECT_TRUE(session.controller->adopt(id, kCopy, std::nullopt));
    session.controller.reset();
    EXPECT_FALSE(copy_exists(folder, kCopy));
}

TEST(RecoveryAdopt, ARefusedAdoptionChangesNothing) {
    Folder folder;
    put_copy(folder, kCopy, "old text");
    put_copy(folder, kOther, "other text");
    Session session(folder.root);
    DocumentId first = session.add_untitled("old text");
    DocumentId second = session.add_untitled("other text");
    EXPECT_FALSE(session.controller->adopt(first, "../escape", std::nullopt)); // not an id
    EXPECT_FALSE(session.controller->adopt(first, "", std::nullopt));
    EXPECT_FALSE(session.controller->adopt(999, kOther, std::nullopt)); // not a document
    EXPECT_TRUE(session.controller->adopt(first, kCopy, std::nullopt));
    EXPECT_FALSE(session.controller->adopt(first, kOther, std::nullopt)); // already has an id
    EXPECT_FALSE(session.controller->adopt(second, kCopy, std::nullopt)); // id is taken
    session.controller.reset();
    EXPECT_FALSE(copy_exists(folder, kCopy));
    EXPECT_TRUE(copy_exists(folder, kOther)); // never adopted, so never deleted
}

TEST(RecoveryAdopt, AnInactiveControllerRefuses) {
    Session session(""); // no usable folder: crash recovery is off
    DocumentId id = session.add_untitled("text");
    EXPECT_FALSE(session.controller->active());
    EXPECT_FALSE(session.controller->adopt(id, kCopy, std::nullopt));
}

// About six seconds on the real clock (the schedule waits five seconds of quiet).
TEST(RecoveryAdopt, TheNextCopyReplacesTheAdoptedFile) {
    Folder folder;
    put_copy(folder, kCopy, "old text");
    Session session(folder.root);
    DocumentId id = session.add_untitled("new text");
    EXPECT_TRUE(session.controller->adopt(id, kCopy, kFileTimeUs));
    session.controller->document_edited(id, 8);
    auto has_new_text = [&] {
        auto copy = store_of(folder).read(kCopy);
        return copy.is_ok() && copy.value().text == "new text";
    };
    EXPECT_TRUE(run_until(has_new_text, 9000));
    EXPECT_EQ(file_count(folder), static_cast<std::size_t>(1)); // no second file for the document
    session.controller.reset();
    EXPECT_FALSE(copy_exists(folder, kCopy));
}

} // namespace
