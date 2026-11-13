// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The main window's part of the start-up question about recovery copies: it gives the prompt its
// Host and nothing else; the walk and its rules are in recovery_prompt.cpp.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/recovery_blank.h"
#include "core/recovery_store.h"
#include "ui/close_dialog.h"
#include "ui/main_window.h"

namespace vermilion::ui {

void MainWindow::offer_recovery() {
    if (prompt_) {
        return;
    }
    RecoveryPrompt::Host host;
    host.ask = [this](const std::string& message, const std::string& detail,
                      const std::vector<std::string>& buttons, std::function<void(int)> on_answer) {
        ask(window_, message, detail, buttons, std::move(on_answer));
    };
    host.close_blank = [this] { close_blank_document(); };
    host.add_document = [this](RecoveryPrompt::Document document) {
        return add_recovered_document(std::move(document));
    };
    host.adopt = [this](DocumentId id, const std::string& recovery_id,
                        std::optional<std::int64_t> file_mtime_us) {
        return recovery_.adopt(id, recovery_id, file_mtime_us);
    };
    host.message = [this](const std::string& message) { status_.show_message(message); };
    prompt_ = std::make_unique<RecoveryPrompt>(
        RecoveryStore(default_recovery_root(), RecoveryProgram::Light), std::move(host));
    prompt_->start();
}

// The blank document of the start is closed once, before the first recovered document is added,
// and only while it is still untouched.
void MainWindow::close_blank_document() {
    if (!blank_id_) {
        return;
    }
    DocumentId id = *blank_id_;
    blank_id_.reset();
    const DocumentRecord* record = list_.find(id); // borrowed
    EditorPage* page = tabs_.editor(id);           // borrowed
    if (record != nullptr && page != nullptr && is_untouched_blank(*record, page->char_count())) {
        tabs_.close(id);
    }
}

RecoveryPrompt::Added MainWindow::add_recovered_document(RecoveryPrompt::Document document) {
    DocumentTabs::Recovered recovered;
    recovered.path = std::move(document.path);
    recovered.format = document.format;
    recovered.mixed_line_endings = document.mixed_line_endings;
    recovered.file_mtime_us = document.file_mtime_us;
    recovered.text = std::move(document.text);
    DocumentId id = tabs_.add_recovered(std::move(recovered));
    const DocumentRecord* record = list_.find(id); // borrowed
    return RecoveryPrompt::Added{id, record != nullptr && !record->path.empty()};
}

} // namespace vermilion::ui
