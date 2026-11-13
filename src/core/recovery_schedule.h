// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// When each edited document gets its next recovery copy: idle time, a cap, one copy at a time.

#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "core/document_list.h"

namespace vermilion {

// Milliseconds on a monotonic clock that the caller reads. The schedule never reads a clock; every
// call that matters is given the time, so tests control it. Any value of the type is accepted.
using Millis = std::int64_t;

// A copy is due this long after the last change, and at the latest this long after the first
// change since the previous copy of that document began.
inline constexpr Millis kRecoveryIdleMs = 5000;
inline constexpr Millis kRecoveryCapMs = 30000;

// After a copy, the next one (of any document) waits this many times as long as the copy held the
// main loop, so copying never takes more than about one twentieth of the time.
inline constexpr Millis kRecoveryBackoffFactor = 20;
// A reported hold longer than this counts as this long (an hour), so the wait stays bounded.
inline constexpr Millis kRecoveryMaxFreezeMs = 3600000;

// The largest document, in characters, that gets recovery copies. Provisional: it is fixed in the
// last step of gate M2.2c from the main-loop hold that copies of a 39 MB document log.
inline constexpr std::uint64_t kRecoveryCeilingChars = 100000000;

// True for a document of this many characters or fewer.
bool recovery_size_allowed(std::uint64_t chars);

// The state machine behind the recovery copies. The editor tells it about changes, asks which
// document is due, and reports each copy as it begins and ends. It holds only times and ids: no
// text, no files, no widgets.
//
// Life of one document: note_change() makes it pending. next_due() names it once its time has
// come. begin_copy() takes it out of the pending set; the editor then takes the text and writes
// the file. A change made while the copy is under way makes the document pending again. end_copy()
// closes the copy. forget() drops a document that no longer needs a copy (saved, closed, or no
// longer editable).
class RecoverySchedule {
public:
    // The document has an unsaved change at time now. The first change since the document was
    // last copied (or forgotten) starts its cap; every change moves its idle time. A time earlier
    // than the last change seen for this document does not move anything back.
    void note_change(DocumentId id, Millis now);

    // The document needs no copy: it is no longer pending. Harmless for an unknown id. If a copy
    // of it is under way, end_copy() will report it as stale.
    void forget(DocumentId id);

    // The document that should be copied now, or nothing. Nothing while a copy is under way and
    // while the back-off after the previous copy lasts (now before the back-off's end). Otherwise,
    // among the pending documents whose time has come (idle time or cap, whichever is earlier, is
    // not after now), the one whose time is earliest; the lowest id when times are equal.
    std::optional<DocumentId> next_due(Millis now) const;

    // The earliest time at which next_due() can name a document if nothing changes: the earliest
    // time of a pending document, or the end of the back-off if that is later. Nothing when no
    // document is pending or while a copy is under way (end_copy() comes first).
    std::optional<Millis> wake_at() const;

    // A copy of this pending document begins. False, changing nothing, when another copy is under
    // way or the document is not pending. On success the document stops being pending.
    bool begin_copy(DocumentId id);

    // The copy that began last is over at time now. freeze_ms is how long it held the main loop
    // (a negative value counts as 0, a value above kRecoveryMaxFreezeMs as that); the next copy
    // waits kRecoveryBackoffFactor times that after now. written tells whether the file was
    // written: when it was not, the document becomes pending again as if changed at now, unless
    // it already is pending again, or was forgotten. Returns true when the document was forgotten
    // while the copy was under way (the file the copy wrote is stale and the caller deletes it).
    // With no copy under way it returns false and changes nothing.
    bool end_copy(Millis now, Millis freeze_ms, bool written);

    bool pending(DocumentId id) const { return pending_.count(id) != 0; }
    bool copy_in_flight() const { return in_flight_.has_value(); }

private:
    struct Pending {
        Millis first = 0; // the first change since the last copy began
        Millis last = 0;  // the latest change
    };

    // The time at which the document is due: idle time after the last change or the cap after the
    // first, whichever is earlier. Sums that would overflow stop at the largest value.
    static Millis due_time(const Pending& pending);

    std::map<DocumentId, Pending> pending_;
    std::optional<DocumentId> in_flight_;
    bool in_flight_forgotten_ = false;
    std::optional<Millis> not_before_; // the end of the back-off; nothing before the first copy
};

} // namespace vermilion
