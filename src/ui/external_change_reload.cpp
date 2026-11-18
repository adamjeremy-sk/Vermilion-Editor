// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// File > Reload from Disk: the user's own reload of a document from its file.

#include <memory>
#include <string>
#include <utility>

#include "core/external_messages.h"
#include "ui/external_change_controller.h"

namespace vermilion::ui {

namespace {

// Keeps a guard ticket until the last copy of the holder is gone. The answer of a question may
// never come (Cancel), and a ticket must be given back exactly once.
class TicketHolder {
public:
    explicit TicketHolder(gpointer ticket) : ticket_(ticket) {}
    ~TicketHolder() { gtkx::AsyncGuard<ExternalChangeController>::free_ticket(ticket_); }

    TicketHolder(const TicketHolder&) = delete;
    TicketHolder& operator=(const TicketHolder&) = delete;

    // The controller, or nullptr once it is gone.
    ExternalChangeController* owner() const {
        return gtkx::AsyncGuard<ExternalChangeController>::peek(ticket_);
    }

private:
    gpointer ticket_;
};

} // namespace

void ExternalChangeController::reload_from_disk(DocumentId id) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || record->path.empty() || !state) {
        return;
    }
    if (!state->quiet || running_.count(id) != 0) {
        host_.message(reload_busy_message(record->title));
        return;
    }
    running_.insert(id);
    std::string path = record->path;
    gpointer ticket = guard_.ticket();
    inspect_file(path, false, [this, id, path, ticket](const FileInspection& seen) {
        if (gtkx::AsyncGuard<ExternalChangeController>::take(ticket) == nullptr) {
            return;
        }
        on_reload_inspected(id, path, seen);
    });
}

void ExternalChangeController::on_reload_inspected(DocumentId id, const std::string& path,
                                                   const FileInspection& seen) {
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state) {
        running_.erase(id);
        wanted_.erase(id);
        return;
    }
    if (record->path != path) {
        running_.erase(id);
        return;
    }
    if (!state->quiet) {
        running_.erase(id);
        host_.message(reload_busy_message(record->title));
        return;
    }
    if (!seen.readable) {
        running_.erase(id);
        host_.message(reload_failed_message(record->title));
        return;
    }
    if (!seen.disk.exists) {
        running_.erase(id);
        host_.message(reload_missing_message(record->title));
        return;
    }
    const ReloadAsk ask =
        reload_ask(record->modified, seen.disk.size, host_.confirm_large_reload());
    if (ask == ReloadAsk::Nothing) {
        by_hand_.insert(id);
        start_scan(id, false);
        return;
    }
    const ReloadQuestion question = reload_question(record->title, ask, seen.disk.size);
    running_.erase(id);
    auto holder = std::make_shared<TicketHolder>(guard_.ticket());
    host_.ask_reload(question.message, question.detail, question.go, ask == ReloadAsk::LargeFile,
                     [holder, id, ask](bool dont_ask) {
                         ExternalChangeController* self = holder->owner();
                         if (self != nullptr) {
                             self->reload_confirmed(id, ask == ReloadAsk::LargeFile && dont_ask);
                         }
                     });
}

void ExternalChangeController::reload_confirmed(DocumentId id, bool stop_asking) {
    if (stop_asking) {
        host_.set_confirm_large_reload(false);
    }
    const DocumentRecord* record = list_.find(id);
    std::optional<ExternalChangeHost::PageState> state = host_.page_state(id);
    if (record == nullptr || !state) {
        return;
    }
    if (!state->quiet || running_.count(id) != 0) {
        host_.message(reload_busy_message(record->title));
        return;
    }
    running_.insert(id);
    by_hand_.insert(id);
    start_scan(id, false);
}

} // namespace vermilion::ui
