// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements DocumentList: untitled numbering, files, paths, formats, neighbour rule, order.

#include "core/document_list.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace vermilion {

DocumentId DocumentList::add_untitled() {
    int number = 1;
    while (std::any_of(records_.begin(), records_.end(),
                       [number](const DocumentRecord& r) { return r.untitled_number == number; })) {
        ++number;
    }
    DocumentRecord record;
    record.id = next_id_++;
    record.title = "Untitled " + std::to_string(number);
    record.untitled_number = number;
    records_.push_back(record);
    active_ = record.id;
    return record.id;
}

DocumentId DocumentList::add_file(const std::string& path, const std::string& title,
                                  const TextFormat& format, bool mixed_line_endings) {
    const DocumentRecord* existing = find_by_path(path);
    if (existing) {
        active_ = existing->id;
        return existing->id;
    }
    DocumentRecord record;
    record.id = next_id_++;
    record.title = title;
    record.path = path;
    record.format = format;
    record.mixed_line_endings = mixed_line_endings;
    records_.push_back(record);
    active_ = record.id;
    return record.id;
}

bool DocumentList::close(DocumentId id) {
    std::optional<std::size_t> index = index_of(id);
    if (!index) {
        return false;
    }
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(*index));
    if (active_ == id) {
        if (records_.empty()) {
            active_.reset();
        } else {
            active_ = records_[*index > 0 ? *index - 1 : 0].id;
        }
    }
    return true;
}

bool DocumentList::set_active(DocumentId id) {
    if (!index_of(id)) {
        return false;
    }
    active_ = id;
    return true;
}

bool DocumentList::set_modified(DocumentId id, bool modified) {
    std::optional<std::size_t> index = index_of(id);
    if (!index) {
        return false;
    }
    records_[*index].modified = modified;
    return true;
}

bool DocumentList::set_read_only(DocumentId id, bool read_only) {
    std::optional<std::size_t> index = index_of(id);
    if (!index) {
        return false;
    }
    records_[*index].read_only = read_only;
    return true;
}

bool DocumentList::set_path(DocumentId id, const std::string& path, const std::string& title) {
    std::optional<std::size_t> index = index_of(id);
    if (!index) {
        return false;
    }
    if (path.empty()) {
        return false;
    }
    const DocumentRecord* same_path = find_by_path(path);
    if (same_path && same_path->id != id) {
        return false;
    }
    DocumentRecord& record = records_[*index];
    record.path = path;
    record.title = title;
    record.untitled_number = 0;
    return true;
}

bool DocumentList::set_format(DocumentId id, const TextFormat& format, bool mixed_line_endings) {
    std::optional<std::size_t> index = index_of(id);
    if (!index) {
        return false;
    }
    records_[*index].format = format;
    records_[*index].mixed_line_endings = mixed_line_endings;
    return true;
}

bool DocumentList::move(DocumentId id, std::size_t index) {
    std::optional<std::size_t> from = index_of(id);
    if (!from) {
        return false;
    }
    DocumentRecord record = std::move(records_[*from]);
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(*from));
    std::size_t to = std::min(index, records_.size());
    records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(to), std::move(record));
    return true;
}

const DocumentRecord* DocumentList::find(DocumentId id) const {
    std::optional<std::size_t> index = index_of(id);
    return index ? &records_[*index] : nullptr;
}

const DocumentRecord* DocumentList::find_by_path(const std::string& path) const {
    if (path.empty()) {
        return nullptr;
    }
    for (const DocumentRecord& record : records_) {
        if (record.path == path) {
            return &record;
        }
    }
    return nullptr;
}

std::optional<std::size_t> DocumentList::index_of(DocumentId id) const {
    for (std::size_t i = 0; i < records_.size(); ++i) {
        if (records_[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

std::size_t DocumentList::modified_count() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(), records_.end(), [](const DocumentRecord& r) { return r.modified; }));
}

std::vector<DocumentId> DocumentList::modified_ids() const {
    std::vector<DocumentId> ids;
    for (const DocumentRecord& record : records_) {
        if (record.modified) {
            ids.push_back(record.id);
        }
    }
    return ids;
}

} // namespace vermilion
