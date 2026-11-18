// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The external-change policy: is_large, needs_hash, decide, classify_look, close_case and
// reload_ask.

#include "core/external_change.h"

namespace vermilion {

namespace {

ExternalAction changed(const DocumentFacts& facts, std::uint64_t disk_size) {
    if (facts.edited) {
        return {ChangeAction::ShowBanner, BannerKind::Changed};
    }
    if (is_large(disk_size)) {
        return {ChangeAction::ShowBanner, BannerKind::ChangedLarge};
    }
    if (facts.reload_would_ask) {
        return {ChangeAction::ShowBanner, BannerKind::Changed};
    }
    return {ChangeAction::ReloadSilently, BannerKind::None};
}

} // namespace

bool is_large(std::uint64_t size) {
    return size > kLargeFileBytes;
}

bool needs_hash(const FileStamp& stamp, const DiskState& disk, bool deleted_on_disk) {
    if (!disk.exists || deleted_on_disk) {
        return false;
    }
    if (stamp.time_only) {
        return disk.mtime_us == stamp.mtime_us;
    }
    return disk.mtime_us != stamp.mtime_us || disk.size != stamp.size;
}

ExternalAction decide(const FileStamp& stamp, const DiskState& disk,
                      const std::optional<std::uint64_t>& hash, const DocumentFacts& facts) {
    if (!disk.exists) {
        if (facts.deleted_on_disk) {
            return {ChangeAction::Ignore, BannerKind::None};
        }
        return {ChangeAction::MarkDeleted, BannerKind::None};
    }
    if (facts.deleted_on_disk) {
        if (!facts.edited && !is_large(disk.size) && !facts.reload_would_ask) {
            return {ChangeAction::ReloadSilently, BannerKind::None};
        }
        return {ChangeAction::ShowBanner, BannerKind::Returned};
    }
    if (stamp.time_only) {
        if (disk.mtime_us != stamp.mtime_us) {
            return changed(facts, disk.size);
        }
        if (hash.has_value()) {
            return {ChangeAction::UpdateStampOnly, BannerKind::None};
        }
        return {ChangeAction::Ignore, BannerKind::None};
    }
    if (disk.mtime_us == stamp.mtime_us && disk.size == stamp.size) {
        return {ChangeAction::Ignore, BannerKind::None};
    }
    if (!hash.has_value()) {
        return {ChangeAction::Ignore, BannerKind::None};
    }
    if (stamp.hash.has_value() && *hash == *stamp.hash) {
        return {ChangeAction::UpdateStampOnly, BannerKind::None};
    }
    return changed(facts, disk.size);
}

FileLook classify_look(const FileStamp& stamp, const DiskState& disk,
                       const std::optional<std::uint64_t>& hash, bool deleted_on_disk) {
    if (!disk.exists) {
        return FileLook::Gone;
    }
    if (deleted_on_disk) {
        return FileLook::Changed;
    }
    if (stamp.time_only) {
        if (disk.mtime_us != stamp.mtime_us) {
            return FileLook::Changed;
        }
        if (hash.has_value()) {
            return FileLook::Same;
        }
        return FileLook::Unknown;
    }
    if (disk.mtime_us == stamp.mtime_us && disk.size == stamp.size) {
        return FileLook::Same;
    }
    if (!hash.has_value()) {
        return FileLook::Unknown;
    }
    if (stamp.hash.has_value() && *hash == *stamp.hash) {
        return FileLook::Same;
    }
    return FileLook::Changed;
}

CloseCase close_case(bool edited, bool deleted_on_disk, FileLook look) {
    if (deleted_on_disk) {
        if (look == FileLook::Changed) {
            return CloseCase::EditedFileChanged;
        }
        return CloseCase::FileGone;
    }
    if (look == FileLook::Gone) {
        return CloseCase::FileGone;
    }
    if (look == FileLook::Changed && edited) {
        return CloseCase::EditedFileChanged;
    }
    return CloseCase::Edited;
}

ReloadAsk reload_ask(bool edited, std::uint64_t disk_size, bool confirm_large) {
    if (edited) {
        return ReloadAsk::DiscardChanges;
    }
    if (confirm_large && is_large(disk_size)) {
        return ReloadAsk::LargeFile;
    }
    return ReloadAsk::Nothing;
}

} // namespace vermilion
