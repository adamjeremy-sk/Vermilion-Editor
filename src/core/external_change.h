// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The policy for a file that changes on disk while its document is open, as pure functions.

#pragma once

#include <cstdint>
#include <optional>

#include "core/file_stamp.h"

namespace vermilion {

// Above this size (10 MiB, the size of the file on disk) a clean document is never reloaded by
// itself: a banner asks instead. At exactly this size it still reloads silently.
inline constexpr std::uint64_t kLargeFileBytes = 10485760;

// What the policy needs to know about the document besides its file.
struct DocumentFacts {
    bool edited = false;           // the user has unsaved changes (the real modified flag)
    bool read_only = false;        // a binary or escaped document; it is always clean
    bool deleted_on_disk = false;  // the file was found missing earlier and the tab kept its text
    bool reload_would_ask = false; // reloading would raise an open-time question (a NUL byte or a
                                   // very long line in the new content)
};

enum class ChangeAction {
    Ignore,          // nothing to do (nothing changed, or nothing could be decided yet)
    UpdateStampOnly, // the file was touched but holds the same bytes: record its new time only
    ReloadSilently,  // reload the document from disk with no question
    ShowBanner,      // show the banner described by ExternalAction::banner
    MarkDeleted,     // the file is gone: keep the tab and text, mark it unsaved
};

enum class BannerKind {
    None,         // no banner (the action is not ShowBanner)
    Changed,      // the file changed on disk: Reload, Keep mine
    ChangedLarge, // a clean document over kLargeFileBytes changed: Reload, Ignore
    Returned,     // the file came back after it was deleted or moved: Reload, Keep mine
};

struct ExternalAction {
    ChangeAction action = ChangeAction::Ignore;
    BannerKind banner = BannerKind::None;
};

// What a read-only look at a document's file found, for the questions before a tab closes and at
// quit (nothing is reloaded or shown by a look).
enum class FileLook {
    Unknown, // nothing could be decided (no page, the page is busy, the file is unreadable, or the
             // document changed while the look was under way)
    Same,    // the file holds what the document last saw (a touch counts as the same)
    Changed, // the file's bytes differ from the document's baseline, or it came back after it was
             // deleted or moved
    Gone,    // the file is missing
};

// The wording of the question before a document closes.
enum class CloseCase {
    Edited,            // the usual "Save changes?" question
    EditedFileChanged, // the same, with the extra line that the file also changed on disk
    FileGone,          // the file was deleted or moved: "Save before closing?"
};

// What File > Reload from Disk must ask before it replaces the document's text.
enum class ReloadAsk {
    Nothing,        // reload at once
    DiscardChanges, // the document has unsaved changes that the reload would throw away
    LargeFile,      // a clean document whose file is over kLargeFileBytes, and the user wants to
                    // be asked about large files
};

// True when `size` is above kLargeFileBytes.
bool is_large(std::uint64_t size);

// Whether the caller must read and hash the file before calling decide(): the file exists, the
// document's file is not marked deleted (a returned file needs no hash), and either the time or the
// size differs from the stamp. For a time-only stamp (crash recovery) it is true when the time
// matches the stored one, so that the size and the hash can be filled in; a different time is
// already a change and needs no hash.
bool needs_hash(const FileStamp& stamp, const DiskState& disk, bool deleted_on_disk);

// The one decision. `hash` is the hash of the file's bytes, or empty when it was not read or the
// read failed (decide() then returns Ignore wherever it needed it: nothing is known, try again
// later). Rules, in this order:
//  1. The file is missing: MarkDeleted, or Ignore when it is already marked deleted.
//  2. The file exists and is marked deleted (it came back): ReloadSilently when the document is not
//     edited, the file is not large and a reload would raise no question; otherwise
//     ShowBanner/Returned.
//  3. Time-only stamp: a different time is a change (go to 5); the same time gives UpdateStampOnly
//     when `hash` is known and Ignore when it is not.
//  4. Normal stamp: the same time and size give Ignore (no hash needed); otherwise no `hash` gives
//     Ignore, an equal hash gives UpdateStampOnly, a different hash is a change (go to 5).
//  5. A change: an edited document gets ShowBanner/Changed (at any size). A clean document over
//     kLargeFileBytes gets ShowBanner/ChangedLarge; a clean document whose reload would raise a
//     question gets ShowBanner/Changed; any other clean document gets ReloadSilently. The size
//     tested is the file's size on disk (`disk.size`). `read_only` does not change the outcome.
ExternalAction decide(const FileStamp& stamp, const DiskState& disk,
                      const std::optional<std::uint64_t>& hash, const DocumentFacts& facts);

// Reads a look at the file as a FileLook. `hash` is the hash of the file's bytes, or empty when it
// was not read (needs_hash() was false) or the read failed. Rules, in this order:
//  1. The file is missing: Gone.
//  2. The document is marked deleted_on_disk and the file exists (it came back): Changed.
//  3. A time-only stamp (crash recovery): a different time is Changed; the same time gives Same
//     when `hash` is known and Unknown when it is not.
//  4. The same time and size as the stamp: Same (no hash needed).
//  5. No `hash`: Unknown.
//  6. A `hash` equal to the stamp's: Same; any other: Changed (also when the stamp has no hash).
FileLook classify_look(const FileStamp& stamp, const DiskState& disk,
                       const std::optional<std::uint64_t>& hash, bool deleted_on_disk);

// Which question a document gets when it closes. Rules, in this order:
//  1. deleted_on_disk: a look of Changed (the file came back) gives EditedFileChanged, any other
//     look FileGone.
//  2. A look of Gone gives FileGone.
//  3. A look of Changed on an edited document gives EditedFileChanged.
//  4. Anything else gives Edited.
CloseCase close_case(bool edited, bool deleted_on_disk, FileLook look);

// What File > Reload from Disk asks for a document. `edited` is the real modified flag,
// `disk_size` the size of the file on disk, `confirm_large` the setting "ask before reloading a
// large file". Rules, in this order:
//  1. An edited document: DiscardChanges (whatever the size and the setting).
//  2. A clean document whose file is over kLargeFileBytes while `confirm_large` is true:
//     LargeFile.
//  3. Anything else: Nothing.
ReloadAsk reload_ask(bool edited, std::uint64_t disk_size, bool confirm_large);

} // namespace vermilion
