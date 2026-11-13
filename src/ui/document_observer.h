// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the document tabs tell a listener: edits, flags, saves, reloads, file times and closing.

#pragma once

#include <cstdint>
#include <optional>

#include "core/document_list.h"

namespace vermilion::ui {

// Implemented by the crash recovery controller (M2.2c STEP 4d). Every call comes from the main
// loop, after DocumentList has been brought up to date. A listener must not add or close a
// document from inside a call.
class DocumentObserver {
public:
    virtual ~DocumentObserver() = default;

    // The text changed (a typed character, a paste, Undo, the byte-order mark), or its format was
    // marked as changed (Encoding, Line Ending); not while a file loads. chars is the length of
    // the text in characters. The record's modified flag may not have caught up yet.
    virtual void document_edited(DocumentId id, std::uint64_t chars) = 0;

    // The buffer's modified flag changed: also when a load ends, and when Undo reaches the saved
    // text again.
    virtual void document_modified(DocumentId id, bool modified) = 0;

    // A save of the document succeeded: the text on screen is the text in the file.
    virtual void document_saved(DocumentId id) = 0;

    // The document's text is about to be replaced by a reload (Encoding > Reopen as).
    virtual void document_reloading(DocumentId id) = 0;

    // The file's modified time as read after a load or a save: microseconds since the Unix epoch,
    // or nothing when it could not be read. It arrives a little after the load or save, and never
    // for a document closed meanwhile.
    virtual void document_synced(DocumentId id, std::optional<std::int64_t> file_mtime_us) = 0;

    // The document is about to be closed; its page still exists.
    virtual void document_closing(DocumentId id) = 0;
};

} // namespace vermilion::ui
