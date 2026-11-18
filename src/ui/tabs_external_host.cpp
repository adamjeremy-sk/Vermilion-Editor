// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The window's side of the external-change controller: it reaches the pages through the tabs.

#include "ui/tabs_external_host.h"

#include <utility>

#include "core/external_messages.h"
#include "core/result.h"
#include "core/settings_keys.h"
#include "ui/check_question.h"
#include "ui/close_dialog.h"

namespace vermilion::ui {

TabsExternalHost::TabsExternalHost(GtkWindow* parent, DocumentTabs& tabs, SettingsFile& settings,
                                   std::function<void(const std::string&)> message)
    : parent_(parent), tabs_(tabs), settings_(settings), message_(std::move(message)) {}

std::optional<ExternalChangeHost::PageState> TabsExternalHost::page_state(DocumentId id) const {
    EditorPage* page = tabs_.editor(id); // borrowed
    if (page == nullptr) {
        return std::nullopt;
    }
    ExternalChangeHost::PageState state;
    state.quiet = page->quiet();
    state.baseline = page->baseline();
    return state;
}

void TabsExternalHost::set_baseline(DocumentId id, const FileStamp& stamp) {
    EditorPage* page = tabs_.editor(id); // borrowed
    if (page != nullptr) {
        page->set_baseline_stamp(stamp);
    }
}

void TabsExternalHost::show_banner(DocumentId id, const std::string& text,
                                   const std::string& primary, const std::string& secondary,
                                   std::function<void()> on_primary,
                                   std::function<void()> on_secondary) {
    EditorPage* page = tabs_.editor(id); // borrowed
    if (page != nullptr) {
        page->show_banner(text, primary, secondary, std::move(on_primary), std::move(on_secondary));
    }
}

void TabsExternalHost::hide_banner(DocumentId id) {
    EditorPage* page = tabs_.editor(id); // borrowed
    if (page != nullptr) {
        page->hide_banner();
    }
}

bool TabsExternalHost::reload(DocumentId id, const ScanResult& scan, const std::string& message) {
    return tabs_.reload(id, scan, message);
}

void TabsExternalHost::mark_modified(DocumentId id) {
    tabs_.mark_modified(id);
}

void TabsExternalHost::deleted_on_disk(DocumentId id) {
    tabs_.note_deleted_on_disk(id);
}

void TabsExternalHost::ask_reload_anyway(const std::string& message, const std::string& detail,
                                         std::function<void()> go) {
    ask_discard(parent_, message, detail, "Reload Anyway", std::move(go));
}

void TabsExternalHost::ask_reload(const std::string& message, const std::string& detail,
                                  const std::string& go_label, bool offer_dont_ask,
                                  std::function<void(bool)> go) {
    if (!offer_dont_ask) {
        ask_discard(parent_, message, detail, go_label, [go = std::move(go)] { go(false); });
        return;
    }
    ask_with_check(parent_, message, detail, go_label, dont_ask_again_label(),
                   [go = std::move(go)](bool chose_go, bool ticked) {
                       if (chose_go) {
                           go(ticked);
                       }
                   });
}

bool TabsExternalHost::confirm_large_reload() const {
    return settings_.get_bool(settings::kFilesGroup, settings::kConfirmLargeReload,
                              settings::kConfirmLargeReloadDefault);
}

void TabsExternalHost::set_confirm_large_reload(bool confirm) {
    settings_.set_bool(settings::kFilesGroup, settings::kConfirmLargeReload, confirm);
    const Status saved = settings_.save();
    if (!saved.is_ok()) {
        message_(settings_save_failed_message(saved.error().message));
    }
}

void TabsExternalHost::message(const std::string& text) {
    message_(text);
}

} // namespace vermilion::ui
