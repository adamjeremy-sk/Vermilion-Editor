// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the recovery tracker: who gets a copy, the copy's header, and which files to delete.

#include "core/recovery_tracker.h"

#include <glib.h>

#include <algorithm>
#include <set>
#include <utility>

#include "core/recovery_store.h"

namespace vermilion {

namespace {

// The "file:///" uri of an absolute path, percent-encoded; nothing for an empty path or a path
// that cannot be a uri (a relative one, say).
std::optional<std::string> uri_of(const std::string& path) {
    if (path.empty()) {
        return std::nullopt;
    }
    g_autoptr(GError) error = nullptr;
    g_autofree char* uri = g_filename_to_uri(path.c_str(), nullptr, &error);
    if (uri == nullptr) {
        return std::nullopt;
    }
    return std::string(uri);
}

// The title of a copy's header: an empty title is "Untitled"; a CR, LF or NUL becomes a space, so
// the title can never break the header line.
std::string header_title(const std::string& title) {
    if (title.empty()) {
        return "Untitled";
    }
    std::string result = title;
    std::replace_if(
        result.begin(), result.end(), [](char c) { return c == '\r' || c == '\n' || c == '\0'; },
        ' ');
    return result;
}

} // namespace

RecoveryTracker::RecoveryTracker(const DocumentList& list, RecoveryIdSource ids)
    : list_(list), ids_(ids ? std::move(ids) : RecoveryIdSource(new_recovery_id)), schedule_() {}

RecoveryEffects RecoveryTracker::note_edit(DocumentId id, std::uint64_t chars, Millis now) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        return {};
    }
    if (record->read_only) {
        return stop_copies(id);
    }
    Entry& entry = entries_[id];
    if (!recovery_size_allowed(chars)) {
        RecoveryEffects effects = stop_copies(id);
        if (!entry.too_large_announced) {
            entry.too_large_announced = true;
            effects.announce_too_large = true;
        }
        return effects;
    }
    entry.too_large_announced = false;
    if (entry.recovery_id.empty()) {
        entry.recovery_id = ids_();
    }
    schedule_.note_change(id, now);
    return {};
}

RecoveryEffects RecoveryTracker::note_clean(DocumentId id) {
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        it->second.too_large_announced = false;
    }
    return stop_copies(id);
}

RecoveryEffects RecoveryTracker::note_closed(DocumentId id) {
    RecoveryEffects effects = stop_copies(id);
    entries_.erase(id);
    return effects;
}

void RecoveryTracker::set_file_mtime(DocumentId id, std::optional<std::int64_t> mtime_us) {
    if (list_.find(id) != nullptr) {
        entries_[id].file_mtime_us = mtime_us;
    }
}

bool RecoveryTracker::adopt_copy(DocumentId id, const std::string& recovery_id,
                                 std::optional<std::int64_t> file_mtime_us) {
    if (list_.find(id) == nullptr || !valid_recovery_id(recovery_id)) {
        return false;
    }
    for (const auto& [entry_id, entry] : entries_) {
        if (entry_id == id) {
            if (!entry.recovery_id.empty()) {
                return false;
            }
        } else if (entry.recovery_id == recovery_id) {
            return false;
        }
    }
    Entry& entry = entries_[id];
    entry.recovery_id = recovery_id;
    entry.file_mtime_us = file_mtime_us;
    entry.copy_exists = true;
    return true;
}

std::optional<Millis> RecoveryTracker::wake_at() const {
    return schedule_.wake_at();
}

std::optional<RecoveryJob> RecoveryTracker::begin_next(Millis now, std::int64_t copied_us) {
    while (std::optional<DocumentId> id = schedule_.next_due(now)) {
        const DocumentRecord* record = list_.find(*id);
        auto it = entries_.find(*id);
        bool needs_copy = record != nullptr && record->modified && !record->read_only &&
                          it != entries_.end() && !it->second.recovery_id.empty();
        if (!needs_copy) {
            schedule_.forget(*id);
            continue;
        }
        schedule_.begin_copy(*id);
        in_flight_ = InFlight{*id, it->second.recovery_id};
        return RecoveryJob{*id, it->second.recovery_id, header_of(*record, it->second, copied_us)};
    }
    return std::nullopt;
}

RecoveryEffects RecoveryTracker::end_copy(Millis now, Millis freeze_ms, bool written) {
    RecoveryEffects effects;
    if (!in_flight_.has_value()) {
        return effects;
    }
    InFlight done = std::move(*in_flight_);
    in_flight_.reset();
    bool stale = schedule_.end_copy(now, freeze_ms, written);
    auto it = entries_.find(done.document);
    if (stale) {
        effects.delete_id = done.recovery_id;
        if (it != entries_.end()) {
            it->second.copy_exists = false;
        }
    } else if (written && it != entries_.end()) {
        it->second.copy_exists = true;
    }
    if (written) {
        failure_announced_ = false;
    } else if (!failure_announced_) {
        failure_announced_ = true;
        effects.announce_failure = true;
    }
    return effects;
}

std::optional<std::string> RecoveryTracker::recovery_id_of(DocumentId id) const {
    auto it = entries_.find(id);
    if (it == entries_.end() || it->second.recovery_id.empty()) {
        return std::nullopt;
    }
    return it->second.recovery_id;
}

std::vector<std::string> RecoveryTracker::copy_ids() const {
    std::set<std::string> ids;
    for (const auto& [id, entry] : entries_) {
        if (entry.copy_exists) {
            ids.insert(entry.recovery_id);
        }
    }
    if (in_flight_.has_value()) {
        ids.insert(in_flight_->recovery_id);
    }
    return std::vector<std::string>(ids.begin(), ids.end());
}

RecoveryEffects RecoveryTracker::stop_copies(DocumentId id) {
    schedule_.forget(id);
    RecoveryEffects effects;
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second.copy_exists) {
        effects.delete_id = it->second.recovery_id;
        it->second.copy_exists = false;
    }
    return effects;
}

RecoveryHeader RecoveryTracker::header_of(const DocumentRecord& record, const Entry& entry,
                                          std::int64_t copied_us) const {
    RecoveryHeader header;
    std::optional<std::string> uri = uri_of(record.path);
    if (uri.has_value()) {
        header.uri = std::move(uri);
        header.file_mtime_us = entry.file_mtime_us;
    } else {
        header.title = header_title(record.title);
    }
    header.copied_us = copied_us;
    header.format = record.format;
    header.mixed_line_endings = record.mixed_line_endings;
    return header;
}

} // namespace vermilion
