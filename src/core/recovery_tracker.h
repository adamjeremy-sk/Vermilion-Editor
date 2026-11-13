// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Which documents get recovery copies, what each copy says about its document, and what to delete.

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_file.h"
#include "core/recovery_schedule.h"

namespace vermilion {

// What the editor must do after a call. Each call sets only the fields its comment names.
struct RecoveryEffects {
    // Delete the file of this copy (RecoveryStore::remove), whether or not it is there.
    std::optional<std::string> delete_id;
    // note_edit(): the document has just gone over the size ceiling; show "Too large for recovery
    // copies" once.
    bool announce_too_large = false;
    // end_copy(): a copy could not be written, and none failed since the last one that worked;
    // show one status message.
    bool announce_failure = false;
};

// One copy to write: the document, the id of its file, and the header for it. header.length is 0:
// RecoveryStore::write sets it from the text.
struct RecoveryJob {
    DocumentId document = 0;
    std::string recovery_id;
    RecoveryHeader header;
};

// Makes the id of a document's recovery file; must give a valid_recovery_id() text.
using RecoveryIdSource = std::function<std::string()>;

// The decisions behind the recovery copies, with no files, widgets or clock: the editor tells it
// what happens to the documents, and asks it what to copy and what to delete. It owns a
// RecoverySchedule and the id of each document's recovery file, which is made once, when the
// document first needs a copy, and kept until the document closes, so a new copy replaces the old
// one. A document has a copy only while it has unsaved changes.
//
// Calls come from the main loop in this order for one copy: begin_next() names the document and
// gives the header; the editor takes the text, writes the file (on another thread if it likes)
// and calls end_copy(). Every time is given, in milliseconds on a monotonic clock (Millis, as in
// recovery_schedule.h), except copied_us, the wall clock in microseconds for the header.
class RecoveryTracker {
public:
    // list is borrowed and must outlive the tracker; the tracker reads it and never changes it.
    // ids is called once per document that needs a copy; without one (the default) the ids are
    // new_recovery_id() UUIDs.
    explicit RecoveryTracker(const DocumentList& list, RecoveryIdSource ids = nullptr);

    // The document's text changed (any edit, also one that only sets the byte-order mark), or its
    // format changed, or its modified flag turned on. chars is the length of its text in
    // characters. An unknown or read-only document needs no copy. A document over the size
    // ceiling (recovery_size_allowed) gets none either: its copy is deleted (delete_id) and
    // announce_too_large is set the first time, until the document is clean again or comes back
    // under the ceiling. Otherwise the document is noted as changed at now. The record's modified
    // flag may not have caught up yet when this is called, so it is checked only when the copy
    // begins.
    RecoveryEffects note_edit(DocumentId id, std::uint64_t chars, Millis now);

    // The document has no unsaved changes any more (saved, or undone back to the saved text), or
    // its text is about to be replaced by a reload. It needs no copy: no more are made, and the
    // copy that exists is to be deleted. If a copy is being written, it is deleted when end_copy()
    // reports it. Harmless for an unknown document or one with no copy.
    RecoveryEffects note_clean(DocumentId id);

    // The document is closed: the same as note_clean(), and the tracker forgets the document and
    // its id.
    RecoveryEffects note_closed(DocumentId id);

    // The modified time of the document's file as of its last load or save, in microseconds since
    // the Unix epoch, or nothing when unknown. Copies of a document with a path carry it. Ignored
    // for a document that is not in the list.
    void set_file_mtime(DocumentId id, std::optional<std::int64_t> mtime_us);

    // The document was recovered from the copy with this id (the start-up question): it takes the
    // id over, so its next copy replaces that file, and the copy counts as existing, so it is
    // deleted when the document is cleaned or closed. file_mtime_us is the file time the copy's
    // text was based on (the copy's header), or nothing. True when it was adopted. False, and
    // nothing changes, for an unknown document, an id that is not valid_recovery_id(), a document
    // that already has an id, and an id that another document uses.
    bool adopt_copy(DocumentId id, const std::string& recovery_id,
                    std::optional<std::int64_t> file_mtime_us);

    // When begin_next() can next name a document (RecoverySchedule::wake_at), or nothing.
    std::optional<Millis> wake_at() const;

    // The next copy to write at time now, or nothing: nothing is due, another copy is under way,
    // or the back-off after the last copy lasts. A document that turned out not to need a copy
    // (not modified, or read-only, by the record) is dropped and the next one is looked at. The
    // header is made from the document's record as it is now: a "file:///" uri of the path
    // (percent-encoded), or, for an untitled document or when the path cannot be made a uri,
    // the title (a CR, LF or NUL in it becomes a space; an empty one "Untitled"); file-mtime only
    // with a uri and only when known; copied_us; the format; the mixed line endings flag.
    std::optional<RecoveryJob> begin_next(Millis now, std::int64_t copied_us);

    // The copy that began last is over at time now; freeze_ms is how long it held the main loop
    // and written says whether its file was written (RecoverySchedule::end_copy). delete_id is set
    // when the document was cleaned or closed meanwhile, so the file is stale. announce_failure is
    // set for a failed write when none failed since the last one that worked. Changes nothing when
    // no copy is under way.
    RecoveryEffects end_copy(Millis now, Millis freeze_ms, bool written);

    bool copy_in_flight() const { return in_flight_.has_value(); }

    // The id of the document's recovery file, or nothing before it first needed a copy.
    std::optional<std::string> recovery_id_of(DocumentId id) const;

    // The ids of every copy that may exist on disk: the ones written and not deleted since, and
    // the one being written. In byte order, each once. For the end of the program, which deletes
    // them all once every document has been answered.
    std::vector<std::string> copy_ids() const;

private:
    struct Entry {
        std::string recovery_id; // empty until the first copy is needed
        std::optional<std::int64_t> file_mtime_us;
        bool copy_exists = false; // a copy was written and has not been deleted
        bool too_large_announced = false;
    };

    struct InFlight {
        DocumentId document = 0;
        std::string recovery_id;
    };

    // No more copies for the document: not pending, and its existing copy is to be deleted.
    RecoveryEffects stop_copies(DocumentId id);
    RecoveryHeader header_of(const DocumentRecord& record, const Entry& entry,
                             std::int64_t copied_us) const;

    const DocumentList& list_;
    RecoveryIdSource ids_;
    RecoverySchedule schedule_;
    std::map<DocumentId, Entry> entries_;
    std::optional<InFlight> in_flight_;
    bool failure_announced_ = false; // a failed write was announced and none worked since
};

} // namespace vermilion
