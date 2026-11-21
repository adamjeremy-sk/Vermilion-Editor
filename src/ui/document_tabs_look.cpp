// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// DocumentTabs::set_look: keeps the look of the editor pages and gives it to the open ones.

#include "ui/document_tabs.h"

namespace vermilion::ui {

void DocumentTabs::set_look(const EditorLook& look) {
    look_ = look;
    for (const DocumentRecord& record : list_.records()) {
        EditorPage* page = editor(record.id); // borrowed
        if (page != nullptr) {
            page->apply_look(look_);
        }
    }
    callbacks_.changed();
}

} // namespace vermilion::ui
