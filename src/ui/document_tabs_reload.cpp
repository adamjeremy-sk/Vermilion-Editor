// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The parts of DocumentTabs that a change of the file on disk uses: reload and the two marks.

#include "ui/document_tabs.h"

#include <string>

#include "core/reload_format.h"

namespace vermilion::ui {

bool DocumentTabs::reload(DocumentId id, const ScanResult& scan, const std::string& message) {
    EditorPage* page = editor(id); // borrowed
    const DocumentRecord* record = list_.find(id);
    if (page == nullptr || record == nullptr || record->path.empty() || page->busy()) {
        return false;
    }
    const std::string path = record->path;
    const ReloadFormat next =
        reload_format(record->format, record->mixed_line_endings, record->encoding_chosen, scan);
    list_.set_format(id, next.format, next.mixed_line_endings);
    list_.set_read_only(id, scan.binary);
    list_.set_deleted_on_disk(id, false);
    reloads_[id] = PendingReload{message, page->view_position()};
    if (observer_ != nullptr) {
        observer_->document_reloading(id);
    }
    page->load(path, next.format.encoding);
    refresh_tab(id);
    callbacks_.changed();
    return true;
}

void DocumentTabs::mark_modified(DocumentId id) {
    EditorPage* page = editor(id); // borrowed
    if (page != nullptr) {
        page->mark_modified();
    }
}

void DocumentTabs::note_deleted_on_disk(DocumentId id) {
    EditorPage* page = editor(id); // borrowed
    if (page == nullptr) {
        return;
    }
    refresh_tab(id);
    if (observer_ != nullptr) {
        observer_->document_deleted_on_disk(id, page->char_count());
    }
    callbacks_.changed();
}

} // namespace vermilion::ui
