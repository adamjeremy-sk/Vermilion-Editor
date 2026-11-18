// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The read-only look at a document's file that the questions before a close and at quit use.

#include <string>
#include <utility>

#include "core/external_messages.h"
#include "ui/external_change_controller.h"

namespace vermilion::ui {

namespace {

bool unchanged_stamp(const FileStamp& a, const FileStamp& b) {
    return a.mtime_us == b.mtime_us && a.size == b.size && a.hash == b.hash &&
           a.time_only == b.time_only;
}

} // namespace

void ExternalChangeController::look(DocumentId id, LookDone done) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || record->path.empty() || !state || !state->quiet || !state->baseline ||
        state->baseline->path != record->path) {
        done(FileLook::Unknown);
        return;
    }
    std::string path = record->path;
    FileStamp stamp = state->baseline->stamp;
    gpointer ticket = guard_.ticket();
    inspect_file(path, false,
                 [this, id, path, stamp, ticket,
                  done = std::move(done)](const FileInspection& seen) mutable {
                     if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
                         return;
                     }
                     on_looked(id, path, stamp, false, seen, std::move(done));
                 });
}

void ExternalChangeController::on_looked(DocumentId id, const std::string& path,
                                         const FileStamp& used, bool hashed,
                                         const FileInspection& seen, LookDone done) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state || record->path != path || !state->quiet || !state->baseline ||
        state->baseline->path != path || !unchanged_stamp(state->baseline->stamp, used) ||
        !seen.readable) {
        done(FileLook::Unknown);
        return;
    }
    if (!hashed && needs_hash(used, seen.disk, record->deleted_on_disk)) {
        gpointer ticket = guard_.ticket();
        inspect_file(path, true,
                     [this, id, path, used, ticket,
                      done = std::move(done)](const FileInspection& with_hash) mutable {
                         if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
                             return;
                         }
                         on_looked(id, path, used, true, with_hash, std::move(done));
                     });
        return;
    }
    const FileLook result = classify_look(used, seen.disk, seen.hash, record->deleted_on_disk);
    if (result == FileLook::Gone && !record->deleted_on_disk) {
        const std::string title = record->title;
        list_.set_deleted_on_disk(id, true);
        host_.hide_banner(id);
        host_.deleted_on_disk(id);
        host_.message(deleted_message(title));
    }
    done(result);
}

} // namespace vermilion::ui
