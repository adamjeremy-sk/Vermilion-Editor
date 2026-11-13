// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The recovery copies found at start-up, their order, and the walk through them.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/recovery_file.h"
#include "core/recovery_store.h"

namespace vermilion {

// One copy found in the recovery folder. A damaged copy has no usable header.
struct RecoveryItem {
    std::string id;
    bool damaged = false;
    std::string problem;   // damaged only: why the file is not a valid copy (for the question)
    RecoveryHeader header; // valid copies only; the text of the copy is not read
};

// Lists the copies in the store and reads each one's header, never its text. Valid copies come
// first, the one copied earliest first and equal times in the order of the ids; damaged copies
// follow, in the order of the ids. A store with no folder, or a folder that cannot be listed (a
// warning is logged), gives an empty list. Each damaged copy is logged as a warning.
std::vector<RecoveryItem> scan_recovery(const RecoveryStore& store);

// The walk through the items scan_recovery() found: the start-up question asks about one copy at
// a time. "number of total" counts the copies in the order they were given, and stays the same
// when a copy is put off. The offer only keeps the position; the editor recovers, deletes or
// keeps the files.
class RecoveryOffer {
public:
    explicit RecoveryOffer(std::vector<RecoveryItem> items);

    // The number of copies the offer started with.
    std::size_t total() const { return items_.size(); }

    // True when no copy is left to ask about.
    bool finished() const { return next_ >= items_.size(); }

    // The copy to ask about now, or nullptr when finished (borrowed; valid until the offer
    // changes).
    const RecoveryItem* current() const;

    // The place of current() among the copies, counted from 1; 0 when finished.
    std::size_t number() const;

    // Recover All is offered when the current copy is valid and, counting it, at least two valid
    // copies are left.
    bool can_recover_all() const;

    // The current copy is dealt with (recovered or discarded): the next one becomes current.
    // Nothing happens when finished.
    void answered();

    // Decide Later: the current copy stays where it is and counts in kept(); the next one becomes
    // current. Nothing happens when finished.
    void decide_later();

    // Recover All: returns the valid copies from the current one on, in order, and the offer is
    // finished. The damaged ones among the rest count in kept(). Empty when finished.
    std::vector<RecoveryItem> take_valid();

    // How many copies were put off or skipped as damaged by take_valid().
    std::size_t kept() const { return kept_; }

private:
    std::vector<RecoveryItem> items_;
    std::size_t next_ = 0;
    std::size_t kept_ = 0;
};

} // namespace vermilion
