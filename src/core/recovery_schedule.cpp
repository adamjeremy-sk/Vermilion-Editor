// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the recovery copy schedule: due times, the cap, the back-off and one copy at a time.

#include "core/recovery_schedule.h"

#include <algorithm>
#include <limits>

namespace vermilion {

namespace {

// a + b, stopping at the largest Millis when the sum would pass it. b is never negative, so the
// overflow is tested before adding (a signed overflow is undefined behaviour).
Millis add_ms(Millis a, Millis b) {
    const Millis max = std::numeric_limits<Millis>::max();
    if (a > max - b) {
        return max;
    }
    return a + b;
}

} // namespace

bool recovery_size_allowed(std::uint64_t chars) {
    return chars <= kRecoveryCeilingChars;
}

Millis RecoverySchedule::due_time(const Pending& pending) {
    return std::min(add_ms(pending.last, kRecoveryIdleMs), add_ms(pending.first, kRecoveryCapMs));
}

void RecoverySchedule::note_change(DocumentId id, Millis now) {
    auto it = pending_.find(id);
    if (it == pending_.end()) {
        pending_.emplace(id, Pending{now, now});
    } else {
        it->second.last = std::max(it->second.last, now);
    }
}

void RecoverySchedule::forget(DocumentId id) {
    pending_.erase(id);
    if (in_flight_ == id) {
        in_flight_forgotten_ = true;
    }
}

std::optional<DocumentId> RecoverySchedule::next_due(Millis now) const {
    if (in_flight_.has_value()) {
        return std::nullopt;
    }
    if (not_before_.has_value() && now < *not_before_) {
        return std::nullopt;
    }
    std::optional<DocumentId> best;
    Millis best_time = 0;
    for (const auto& entry : pending_) {
        const Millis time = due_time(entry.second);
        if (time > now) {
            continue;
        }
        if (!best.has_value() || time < best_time) {
            best = entry.first;
            best_time = time;
        }
    }
    return best;
}

std::optional<Millis> RecoverySchedule::wake_at() const {
    if (in_flight_.has_value()) {
        return std::nullopt;
    }
    if (pending_.empty()) {
        return std::nullopt;
    }
    Millis earliest = 0;
    bool first = true;
    for (const auto& entry : pending_) {
        const Millis time = due_time(entry.second);
        if (first || time < earliest) {
            earliest = time;
            first = false;
        }
    }
    if (not_before_.has_value() && *not_before_ > earliest) {
        earliest = *not_before_;
    }
    return earliest;
}

bool RecoverySchedule::begin_copy(DocumentId id) {
    if (in_flight_.has_value() || pending_.count(id) == 0) {
        return false;
    }
    pending_.erase(id);
    in_flight_ = id;
    in_flight_forgotten_ = false;
    return true;
}

bool RecoverySchedule::end_copy(Millis now, Millis freeze_ms, bool written) {
    if (!in_flight_.has_value()) {
        return false;
    }
    const DocumentId id = *in_flight_;
    const bool forgotten = in_flight_forgotten_;
    in_flight_.reset();
    in_flight_forgotten_ = false;
    const Millis freeze = std::clamp(freeze_ms, Millis(0), kRecoveryMaxFreezeMs);
    not_before_ = add_ms(now, freeze * kRecoveryBackoffFactor);
    if (!written && !forgotten) {
        pending_.emplace(id, Pending{now, now});
    }
    return forgotten;
}

} // namespace vermilion
