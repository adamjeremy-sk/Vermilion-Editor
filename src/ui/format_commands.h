// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The status bar's Encoding and Line Ending menus: reopen in an encoding, or change the format.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

#include "core/document_list.h"
#include "ui/document_tabs.h"

namespace vermilion::ui {

class FormatCommands {
public:
    struct Callbacks {
        std::function<void()> changed;                   // the format of a document changed
        std::function<void(const std::string&)> message; // a line for the status bar
        // Saves a document (File > Save); done gets true once it is saved.
        std::function<void(DocumentId, std::function<void(bool)>)> save;
    };

    // parent, list and tabs are borrowed and must outlive this object.
    FormatCommands(GtkWindow* parent, DocumentList& list, DocumentTabs& tabs, Callbacks callbacks);

    FormatCommands(const FormatCommands&) = delete;
    FormatCommands& operator=(const FormatCommands&) = delete;

    // Encoding > Reopen as: reloads the file in that encoding. A document with unsaved changes
    // asks first: Save and Reopen, Discard Changes and Reopen, or Cancel. Ids as in
    // core/text_format.h.
    void reopen_as(DocumentId id, const std::string& encoding_id);

    // Encoding > Save as: the next save writes that encoding (and byte-order mark).
    void save_with_encoding(DocumentId id, const std::string& encoding_id);

    // Line Ending menu: the next save writes that line ending on every line.
    void set_line_ending(DocumentId id, const std::string& line_ending_id);

private:
    GtkWindow* parent_; // borrowed
    DocumentList& list_;
    DocumentTabs& tabs_;
    Callbacks callbacks_;
};

} // namespace vermilion::ui
