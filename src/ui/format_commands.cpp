// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Encoding > Reopen as and Save as, and the Line Ending menu, applied to one document.

#include "ui/format_commands.h"

#include <optional>
#include <utility>

#include "core/save_messages.h"
#include "ui/close_dialog.h"
#include "ui/editor_page.h"

namespace vermilion::ui {

FormatCommands::FormatCommands(GtkWindow* parent, DocumentList& list, DocumentTabs& tabs,
                               Callbacks callbacks)
    : parent_(parent), list_(list), tabs_(tabs), callbacks_(std::move(callbacks)) {}

void FormatCommands::reopen_as(DocumentId id, const std::string& encoding_id) {
    const DocumentRecord* record = list_.find(id);
    EditorPage* editor = tabs_.editor(id);
    if (record == nullptr || editor == nullptr || record->path.empty()) {
        return;
    }
    if (editor->busy()) {
        callbacks_.message(busy_message(record->title));
        return;
    }
    std::optional<TextFormat> format = format_with_encoding(record->format, encoding_id);
    if (!format) {
        return;
    }
    auto reload = [this, id, chosen = *format] {
        const DocumentRecord* current = list_.find(id);
        if (current != nullptr) {
            list_.set_format(id, chosen, current->mixed_line_endings);
            tabs_.reopen(id, chosen.encoding);
        }
    };
    if (!record->modified) {
        reload();
        return;
    }
    OpenQuestion question = reopen_question(record->title, encoding_label(*format));
    ask(parent_, question.message, question.detail,
        {"Save and Reopen", "Discard Changes and Reopen", "Cancel"},
        [this, id, reload](int button) {
            if (button == 0) {
                callbacks_.save(id, [reload](bool saved) {
                    if (saved) {
                        reload();
                    }
                });
            } else if (button == 1) {
                reload();
            }
        });
}

// The byte-order mark lives in the text, so changing it is an edit; any other change marks the
// text as changed so that the next save writes the new format.
void FormatCommands::save_with_encoding(DocumentId id, const std::string& encoding_id) {
    const DocumentRecord* record = list_.find(id);
    EditorPage* editor = tabs_.editor(id);
    if (record == nullptr || editor == nullptr || record->read_only || editor->busy()) {
        return;
    }
    std::optional<TextFormat> format = format_with_encoding(record->format, encoding_id);
    if (!format) {
        return;
    }
    list_.set_format(id, *format, record->mixed_line_endings);
    if (format->bom != editor->starts_with_bom()) {
        editor->set_bom(format->bom);
    } else {
        editor->mark_modified();
    }
    callbacks_.message(next_save_message(*format));
    callbacks_.changed();
}

void FormatCommands::set_line_ending(DocumentId id, const std::string& line_ending_id) {
    const DocumentRecord* record = list_.find(id);
    EditorPage* editor = tabs_.editor(id);
    std::optional<LineEnding> ending = line_ending_from_id(line_ending_id);
    if (record == nullptr || editor == nullptr || record->read_only || editor->busy() || !ending) {
        return;
    }
    TextFormat format = record->format;
    format.line_ending = *ending;
    list_.set_format(id, format, false);
    editor->mark_modified();
    callbacks_.message(next_save_message(format));
    callbacks_.changed();
}

} // namespace vermilion::ui
