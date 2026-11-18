// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the recovery copy of a document whose file was deleted or moved, on a real main loop.

#include <gtest/gtest.h>

#include <glib.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "core/document_list.h"
#include "core/recovery_store.h"
#include "core/text_format.h"
#include "ui/recovery_controller.h"

namespace {

using vermilion::DocumentId;
using vermilion::DocumentList;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::TextFormat;
using vermilion::ui::RecoveryController;

// A temporary directory as the recovery root, removed with the object.
struct Folder {
    Folder() {
        g_autofree char* made = g_dir_make_tmp("vermilion-deleted-XXXXXX", nullptr);
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

// The number of copy files in the folder, or 9999 when it cannot be read.
std::size_t file_count(const Folder& folder) {
    auto ids = RecoveryStore(folder.root, RecoveryProgram::Light).list();
    return ids.is_ok() ? ids.value().size() : 9999;
}

// The text of the only copy in the folder, or nothing.
std::optional<std::string> only_copy_text(const Folder& folder) {
    RecoveryStore store(folder.root, RecoveryProgram::Light);
    auto ids = store.list();
    if (!ids.is_ok() || ids.value().size() != 1) {
        return std::nullopt;
    }
    auto copy = store.read(ids.value()[0]);
    if (!copy.is_ok()) {
        return std::nullopt;
    }
    return copy.value().text;
}

// Runs the main loop until done() holds or the limit passes; true when done() holds.
template <typename Done>
bool run_until(Done done, int limit_ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(limit_ms) * 1000;
    while (!done() && g_get_monotonic_time() < end) {
        g_main_context_iteration(g_main_context_default(), FALSE);
        g_usleep(5000);
    }
    return done();
}

// Runs the main loop for a while without waiting for anything.
void run_for(int ms) {
    run_until([] { return false; }, ms);
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

    // A clean file document whose file has just been found missing.
    DocumentId add_deleted_file(const std::string& text) {
        DocumentId id = list.add_file("/tmp/gone.txt", "gone.txt", TextFormat{}, false);
        list.set_deleted_on_disk(id, true);
        texts[id] = text;
        return id;
    }

    DocumentList list;
    std::map<DocumentId, std::string> texts;
    std::unique_ptr<RecoveryController> controller; // last: it goes first
};

// One run of about six seconds (the schedule waits five seconds of quiet before it copies).
TEST(RecoveryDeleted, ADeletedDocumentKeepsItsCopyUntilItIsReloaded) {
    Folder folder;
    Session session(folder.root);
    DocumentId id = session.add_deleted_file("text that exists nowhere else");
    session.controller->document_deleted_on_disk(id, 28);
    ASSERT_TRUE(run_until([&] { return file_count(folder) == 1; }, 12000));
    EXPECT_EQ(only_copy_text(folder), std::optional<std::string>("text that exists nowhere else"));

    // The buffer's modified flag turning off must not take the copy away: the file is still gone.
    session.controller->document_modified(id, false);
    run_for(400);
    EXPECT_EQ(file_count(folder), 1u);

    // The file came back and the document is reloaded: the copy is not needed any more.
    session.list.set_deleted_on_disk(id, false);
    session.controller->document_reloading(id);
    EXPECT_TRUE(run_until([&] { return file_count(folder) == 0; }, 3000));
}

TEST(RecoveryDeleted, ACleanDocumentWhoseFileIsBackStillLosesItsCopyWhenCleaned) {
    Folder folder;
    Session session(folder.root);
    DocumentId id = session.add_deleted_file("kept");
    session.controller->document_deleted_on_disk(id, 4);
    session.list.set_deleted_on_disk(id, false); // the file came back before any copy was due
    session.controller->document_modified(id, false);
    run_for(300);
    EXPECT_EQ(file_count(folder), 0u);
}

TEST(RecoveryDeleted, AnUnknownDocumentIsIgnored) {
    Folder folder;
    Session session(folder.root);
    session.controller->document_deleted_on_disk(9999, 10);
    run_for(100);
    EXPECT_EQ(file_count(folder), 0u);
}

} // namespace
