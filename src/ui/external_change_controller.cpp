// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Looks at a document's file when it may have changed, and applies the policy of external_change.h.

#include "ui/external_change_controller.h"

#include <utility>

#include "core/external_messages.h"
#include "core/open_messages.h"
#include "core/text_format.h"

namespace vermilion::ui {

namespace {

bool same_stamp(const FileStamp& a, const FileStamp& b) {
    return a.mtime_us == b.mtime_us && a.size == b.size && a.hash == b.hash &&
           a.time_only == b.time_only;
}

} // namespace

ExternalChangeController::ExternalChangeController(DocumentList& list, ExternalChangeHost& host,
                                                   std::unique_ptr<DebounceTimer> timer,
                                                   unsigned delay_ms)
    : list_(list), host_(host), watcher_(std::move(timer), [this] { on_look(); }, delay_ms) {}

ExternalChangeController::~ExternalChangeController() = default;

void ExternalChangeController::follow_active() {
    std::optional<DocumentId> active = list_.active();
    const DocumentRecord* record = active ? list_.find(*active) : nullptr;
    if (!active || record == nullptr || record->path.empty()) {
        watcher_.stop();
        watched_ = 0;
        return;
    }
    bool switched = watched_ != *active;
    watcher_.watch(record->path);
    watched_ = *active;
    if (switched || wanted_.count(*active) != 0) {
        check(*active);
    }
}

void ExternalChangeController::on_look() {
    if (watched_ != 0) {
        check(watched_);
    }
}

void ExternalChangeController::settled(DocumentId id) {
    if (wanted_.count(id) != 0) {
        check(id);
    }
}

void ExternalChangeController::check(DocumentId id) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || record->path.empty() || !state) {
        wanted_.erase(id);
        return;
    }
    if (!state->quiet || running_.count(id) != 0) {
        wanted_.insert(id);
        return;
    }
    wanted_.erase(id);
    if (!state->baseline || state->baseline->path != record->path) {
        return;
    }
    running_.insert(id);
    std::string path = record->path;
    gpointer ticket = guard_.ticket();
    inspect_file(path, false, [this, id, path, ticket](const FileInspection& seen) {
        if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
            return;
        }
        on_stat(id, path, seen);
    });
}

void ExternalChangeController::on_stat(DocumentId id, const std::string& path,
                                       const FileInspection& seen) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    if (record->path != path || !state->quiet || !state->baseline ||
        state->baseline->path != path) {
        running_.erase(id);
        wanted_.insert(id);
        return;
    }
    if (!seen.readable) {
        finish(id);
        return;
    }
    FileStamp stamp = state->baseline->stamp;
    if (!needs_hash(stamp, seen.disk, record->deleted_on_disk)) {
        conclude(id, stamp, seen);
        return;
    }
    gpointer ticket = guard_.ticket();
    inspect_file(path, true, [this, id, path, stamp, ticket](const FileInspection& hashed) {
        if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
            return;
        }
        on_hashed(id, path, stamp, hashed);
    });
}

void ExternalChangeController::on_hashed(DocumentId id, const std::string& path,
                                         const FileStamp& used, const FileInspection& seen) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    if (record->path != path || !state->quiet || !state->baseline ||
        !same_stamp(state->baseline->stamp, used)) {
        running_.erase(id);
        wanted_.insert(id);
        return;
    }
    if (!seen.readable) {
        finish(id);
        return;
    }
    conclude(id, used, seen);
}

void ExternalChangeController::conclude(DocumentId id, const FileStamp& stamp,
                                        const FileInspection& seen) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    DocumentFacts facts;
    facts.edited = record->modified;
    facts.read_only = record->read_only;
    facts.deleted_on_disk = record->deleted_on_disk;
    const ExternalAction action = decide(stamp, seen.disk, seen.hash, facts);
    switch (action.action) {
    case ChangeAction::Ignore:
        host_.hide_banner(id);
        finish(id);
        break;
    case ChangeAction::UpdateStampOnly: {
        FileStamp fresh;
        fresh.mtime_us = seen.disk.mtime_us;
        fresh.size = seen.disk.size;
        fresh.hash = seen.hash;
        host_.set_baseline(id, fresh);
        host_.hide_banner(id);
        finish(id);
        break;
    }
    case ChangeAction::ReloadSilently:
        start_scan(id, true);
        break;
    case ChangeAction::ShowBanner:
        show_banner(id, action.banner);
        finish(id);
        break;
    case ChangeAction::MarkDeleted: {
        const std::string title = record->title;
        list_.set_deleted_on_disk(id, true);
        host_.hide_banner(id);
        host_.deleted_on_disk(id);
        host_.message(deleted_message(title));
        finish(id);
        break;
    }
    }
}

void ExternalChangeController::finish(DocumentId id) {
    running_.erase(id);
    if (wanted_.count(id) != 0) {
        check(id);
    }
}

void ExternalChangeController::show_banner(DocumentId id, BannerKind kind) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    const BannerButtons buttons = banner_buttons(kind);
    host_.show_banner(
        id, banner_text(kind, record->title), buttons.reload, buttons.secondary,
        [this, id] { on_banner_reload(id); }, [this, id, kind] { on_banner_secondary(id, kind); });
}

void ExternalChangeController::start_scan(DocumentId id, bool silent) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        running_.erase(id);
        return;
    }
    std::string path = record->path;
    gpointer ticket = guard_.ticket();
    probes_[id] = std::make_unique<FileProbe>(
        path, FileProbe::Callbacks{[](int) {},
                                   [this, id, silent, ticket](const FileProbe::Result& result) {
                                       if (gtkx::AsyncGuard<ExternalChangeController>::take(
                                               ticket) == nullptr) {
                                           return;
                                       }
                                       on_scanned(id, silent, result);
                                   }});
}

void ExternalChangeController::on_scanned(DocumentId id, bool silent,
                                          const FileProbe::Result& result) {
    FileProbe::Result scanned = result;
    probes_.erase(id);
    const bool by_hand = by_hand_.erase(id) != 0;
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    if (!scanned.ok) {
        running_.erase(id);
        host_.message(reload_failed_message(record->title));
        return;
    }
    const OpenWarning warning = open_warning(scanned.scan, long_line_limit_);
    if (silent) {
        if (!state->quiet) {
            running_.erase(id);
            wanted_.insert(id);
            return;
        }
        if (record->modified) {
            running_.erase(id);
            check(id);
            return;
        }
        if (warning != OpenWarning::None) {
            show_banner(id, record->deleted_on_disk ? BannerKind::Returned : BannerKind::Changed);
            running_.erase(id);
            return;
        }
        do_reload(id, scanned.scan, by_hand);
        return;
    }
    if (warning != OpenWarning::None) {
        const OpenQuestion question =
            open_question(record->title, warning, scanned.scan.longest_line);
        running_.erase(id);
        ScanResult scan = scanned.scan;
        host_.ask_reload_anyway(question.message, question.detail,
                                [this, id, by_hand, scan = std::move(scan)] {
                                    if (running_.count(id) != 0) {
                                        return;
                                    }
                                    running_.insert(id);
                                    do_reload(id, scan, by_hand);
                                });
        return;
    }
    do_reload(id, scanned.scan, by_hand);
}

void ExternalChangeController::do_reload(DocumentId id, const ScanResult& scan, bool by_hand) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        running_.erase(id);
        return;
    }
    std::string message =
        by_hand ? reloaded_by_hand_message(record->title) : reloaded_message(record->title);
    if (!record->encoding_chosen && encoding_label(scan.format) != encoding_label(record->format)) {
        const std::string label = encoding_label(scan.format);
        message = by_hand ? reloaded_by_hand_encoding_message(record->title, label)
                          : reloaded_encoding_message(record->title, label);
    }
    if (host_.reload(id, scan, message)) {
        host_.hide_banner(id);
    } else {
        wanted_.insert(id);
    }
    running_.erase(id);
}

void ExternalChangeController::on_banner_reload(DocumentId id) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr || running_.count(id) != 0) {
        return;
    }
    running_.insert(id);
    start_scan(id, false);
}

void ExternalChangeController::on_banner_secondary(DocumentId id, BannerKind kind) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr || running_.count(id) != 0) {
        return;
    }
    running_.insert(id);
    std::string path = record->path;
    gpointer ticket = guard_.ticket();
    inspect_file(path, true, [this, id, kind, ticket](const FileInspection& seen) {
        if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
            return;
        }
        const DocumentRecord* record = list_.find(id);
        if (record == nullptr || !host_.page_state(id)) {
            running_.erase(id);
            wanted_.erase(id);
            return;
        }
        if (!seen.readable || !seen.disk.exists) {
            wanted_.insert(id);
            finish(id);
            return;
        }
        FileStamp stamp;
        stamp.mtime_us = seen.disk.mtime_us;
        stamp.size = seen.disk.size;
        stamp.hash = seen.hash;
        const bool was_modified = record->modified;
        host_.set_baseline(id, stamp);
        if (kind == BannerKind::Returned) {
            list_.set_deleted_on_disk(id, false);
            if (!was_modified) {
                host_.mark_modified(id);
            }
        }
        host_.hide_banner(id);
        running_.erase(id);
    });
}

} // namespace vermilion::ui
