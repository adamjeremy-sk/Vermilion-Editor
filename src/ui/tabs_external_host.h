// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The window's side of the external-change controller: it reaches the pages through the tabs.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <optional>
#include <string>

#include "core/document_list.h"
#include "core/file_stamp.h"
#include "core/settings_file.h"
#include "core/text_scan.h"
#include "ui/document_tabs.h"
#include "ui/external_change_controller.h"

namespace vermilion::ui {

// Implements ExternalChangeHost over the document tabs: every call is a thin step to the editor
// page of the document (or to the tabs), a question in the main window, or the settings file. It
// keeps no state of its own.
class TabsExternalHost : public ExternalChangeHost {
public:
    // parent (the window the questions belong to), tabs and settings are borrowed and must outlive
    // this object. message shows a line on the status bar.
    TabsExternalHost(GtkWindow* parent, DocumentTabs& tabs, SettingsFile& settings,
                     std::function<void(const std::string&)> message);

    TabsExternalHost(const TabsExternalHost&) = delete;
    TabsExternalHost& operator=(const TabsExternalHost&) = delete;

    std::optional<PageState> page_state(DocumentId id) const override;
    void set_baseline(DocumentId id, const FileStamp& stamp) override;
    void show_banner(DocumentId id, const std::string& text, const std::string& primary,
                     const std::string& secondary, std::function<void()> on_primary,
                     std::function<void()> on_secondary) override;
    void hide_banner(DocumentId id) override;
    bool reload(DocumentId id, const ScanResult& scan, const std::string& message) override;
    void mark_modified(DocumentId id) override;
    void deleted_on_disk(DocumentId id) override;
    void ask_reload_anyway(const std::string& message, const std::string& detail,
                           std::function<void()> go) override;
    void ask_reload(const std::string& message, const std::string& detail,
                    const std::string& go_label, bool offer_dont_ask,
                    std::function<void(bool)> go) override;
    bool confirm_large_reload() const override;
    void set_confirm_large_reload(bool confirm) override;
    void message(const std::string& text) override;

private:
    GtkWindow* parent_;
    DocumentTabs& tabs_;
    SettingsFile& settings_;
    std::function<void(const std::string&)> message_;
};

} // namespace vermilion::ui
