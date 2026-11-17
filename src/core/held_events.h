// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Holds back a document's edit and cursor notifications during a job and hands them over once.

#pragma once

namespace vermilion {

// A Replace All job edits the text many thousand times a second; telling the status bar and the
// recovery copies after every edit would cost more than the edits. While a hold lasts the page only
// remembers that an edit or a cursor move happened, and release() says which kinds did, so that the
// page reports each kind once at the end. Display-free; everything runs on the main loop.
class HeldEvents {
public:
    // What came in while the hold lasted: a kind is true when it came in at least once.
    struct Pending {
        bool edited = false;
        bool cursor = false;
    };

    bool holding() const { return holding_; }

    // Starts holding with nothing remembered. Holding already: nothing changes, and what was
    // remembered stays.
    void hold() {
        if (!holding_) {
            holding_ = true;
            pending_ = Pending();
        }
    }

    // An edit happened. True when it was held back, so that the caller does not report it; false
    // when not holding, so that the caller reports it at once.
    bool hold_edited() {
        if (holding_) {
            pending_.edited = true;
        }
        return holding_;
    }

    // The cursor moved. True when it was held back, false when the caller reports it at once.
    bool hold_cursor() {
        if (holding_) {
            pending_.cursor = true;
        }
        return holding_;
    }

    // Stops holding and returns what was held back (each kind at most once). Not holding: nothing
    // is returned and nothing changes.
    Pending release() {
        Pending released;
        if (holding_) {
            released = pending_;
            pending_ = Pending();
            holding_ = false;
        }
        return released;
    }

private:
    bool holding_ = false;
    Pending pending_;
};

} // namespace vermilion
