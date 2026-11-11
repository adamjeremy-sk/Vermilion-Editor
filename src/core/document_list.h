// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Open documents in tab order: titles, paths, formats, flags, the active one, untitled numbers.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/text_format.h"

namespace vermilion {

using DocumentId = std::uint64_t;

struct DocumentRecord {
    DocumentId id = 0;
    std::string title;
    int untitled_number = 0; // 1, 2, ... for an untitled document; 0 for a file
    bool modified = false;
    std::string path;  // empty for an untitled document; as the file system names it (not UTF-8)
    TextFormat format; // an untitled document is UTF-8 without a byte-order mark, LF
    bool mixed_line_endings = false; // the file had more than one kind of line ending
    bool read_only = false; // some bytes could not be read as text; the document is never saved
};

// Bookkeeping for the editor's document tabs; the UI keeps one tab per record, in the same order.
// Ids are never reused.
class DocumentList {
public:
    // Appends "Untitled N" with the lowest N no open document uses, and makes it active.
    DocumentId add_untitled();

    // Appends a document for the file at path (absolute), titled title, and makes it active. If a
    // document with this path is open, nothing is added: that one becomes active and its id is
    // returned.
    DocumentId add_file(const std::string& path, const std::string& title, const TextFormat& format,
                        bool mixed_line_endings);

    // Removes the document. If it was active, the neighbour to its left becomes active, or the
    // one to its right when it was the first; when no document is left, none is active.
    bool close(DocumentId id);

    bool set_active(DocumentId id);

    bool set_modified(DocumentId id, bool modified);

    bool set_read_only(DocumentId id, bool read_only);

    // After Save As: the document now lives at path (absolute) under title, and is no longer
    // untitled (its number is free again). False for an unknown id, an empty path, or a path that
    // another open document has.
    bool set_path(DocumentId id, const std::string& path, const std::string& title);

    // The format the next save writes, and whether the text still has mixed line endings (false
    // once a line ending is chosen or a save has written one ending on every line).
    bool set_format(DocumentId id, const TextFormat& format, bool mixed_line_endings);

    // Moves the document to position index (past the end means last), as after a tab drag.
    bool move(DocumentId id, std::size_t index);

    std::optional<DocumentId> active() const { return active_; }

    const DocumentRecord* find(DocumentId id) const;

    // The open document with this path, or nullptr; an empty path finds nothing.
    const DocumentRecord* find_by_path(const std::string& path) const;

    std::optional<std::size_t> index_of(DocumentId id) const;

    const std::vector<DocumentRecord>& records() const { return records_; }

    bool empty() const { return records_.empty(); }

    std::size_t modified_count() const;

    // Documents with unsaved changes, in tab order.
    std::vector<DocumentId> modified_ids() const;

private:
    std::vector<DocumentRecord> records_;
    std::optional<DocumentId> active_;
    DocumentId next_id_ = 1;
};

} // namespace vermilion
