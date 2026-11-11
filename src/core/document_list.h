// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Open documents in tab order: ids, titles, modified flags, the active one and untitled numbers.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vermilion {

using DocumentId = std::uint64_t;

struct DocumentRecord {
    DocumentId id = 0;
    std::string title;
    int untitled_number = 0; // 1, 2, ... for an untitled document
    bool modified = false;
};

// Bookkeeping for the editor's document tabs; the UI keeps one tab per record, in the same order.
// Ids are never reused. File paths arrive with opening files (M2.2).
class DocumentList {
public:
    // Appends "Untitled N" with the lowest N no open document uses, and makes it active.
    DocumentId add_untitled();

    // Removes the document. If it was active, the neighbour to its left becomes active, or the
    // one to its right when it was the first; when no document is left, none is active.
    bool close(DocumentId id);

    bool set_active(DocumentId id);

    bool set_modified(DocumentId id, bool modified);

    // Moves the document to position index (past the end means last), as after a tab drag.
    bool move(DocumentId id, std::size_t index);

    std::optional<DocumentId> active() const { return active_; }

    const DocumentRecord* find(DocumentId id) const;

    std::optional<std::size_t> index_of(DocumentId id) const;

    const std::vector<DocumentRecord>& records() const { return records_; }

    bool empty() const { return records_.empty(); }

    std::size_t modified_count() const;

private:
    std::vector<DocumentRecord> records_;
    std::optional<DocumentId> active_;
    DocumentId next_id_ = 1;
};

} // namespace vermilion
