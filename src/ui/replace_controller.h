// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the Replace window does that is not drawing: the replacement text, the scope and Replace.

#pragma once

#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "ui/debounce_timer.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/search_controller.h"
#include "ui/search_session.h"

namespace vermilion::ui {

// Where Replace All works (Adam, 2026-11-13, design talk topic 5). Replace and Find Next always
// work on the whole document.
enum class ReplaceScope { WholeDocument, SelectionOnly };

// What start_replace_all() did.
enum class ReplaceStart {
    // A job runs; its first slice follows after the gap.
    Started,
    // The pattern or the replacement text is not valid: status() says why; nothing was done.
    Refused,
    // Nothing to do, so no job was started: there is no buffer (nothing is shown), or the search
    // is not active or the range is empty (the status line says "Nothing to replace").
    NothingToReplace,
    // A job is already running.
    Busy
};

// How a Replace All job ended.
enum class ReplaceJobEnd {
    Finished,     // every match in the range was replaced (or there was none)
    Cancelled,    // cancel_job(): what was replaced so far stays
    BufferChanged // the search moved to another buffer (a tab change, or the last tab closed)
};

// What the job-done callback is given.
struct ReplaceJobResult {
    ReplaceJobEnd end = ReplaceJobEnd::Finished;
    std::uint64_t count = 0;   // the matches replaced, in all slices
    bool in_selection = false; // the job worked on the remembered range ("Selection only")
};

// How a job paces itself on the main loop (Adam, 2026-11-16).
struct ReplacePacing {
    // A slice stops at the first time check after this many milliseconds (every 16 matches); 0
    // means no time limit.
    unsigned slice_ms = 25;
    // The pause between two slices and before the first; below 1 it is raised to 1 (a zero-delay
    // timeout would starve the redraw).
    unsigned gap_ms = 5;
    // A slice also stops after this many matches; 0 means no limit (the tests set small values).
    std::uint64_t max_matches = 0;
};

// The Replace window's half of the shared search (the Find window's is the SearchController
// alone). It holds what only the Replace window has: the replacement text and the scope, and it
// runs Replace and Replace All on the controller's session. It creates no widget and includes no
// window code, so it is tested without a display. Everything runs on the main loop.
//
// "Selection only" works on a range that is remembered as two text marks, so that it follows the
// edits that Replace makes inside it. The range is taken from the document's selection when the
// window opens (capture_selection) and when the user chooses the scope (choose_scope); Find Next
// and Replace move the selection but never change the range. A range belongs to one buffer: after
// the controller moves to another buffer it is gone.
//
// Replace All runs as a job in slices (start_replace_all, Adam 2026-11-16), so that the window
// stays alive on a very large text. The job is one undo step; cancelling it, or a change of
// buffer, keeps what was replaced. While a job runs, replace() does nothing, and the caller must
// not edit the buffer or change the search options (the window locks it).
class ReplaceController {
public:
    // `search` (borrowed) must outlive this object. `job_timer` is the scheduler of the job: every
    // slice is one start() of it (the tests give a fake).
    ReplaceController(SearchController& search, std::unique_ptr<DebounceTimer> job_timer);

    // Ends a running job without calling the callbacks, then removes the range's marks from the
    // buffer when they are still there.
    ~ReplaceController();

    ReplaceController(const ReplaceController&) = delete;
    ReplaceController& operator=(const ReplaceController&) = delete;

    // The text that Replace and Replace All put in. Literal when the search is not a regular
    // expression; otherwise GLib's replacement syntax applies (core/search_replacement.h).
    void set_replacement(const std::string& text);
    const std::string& replacement() const { return replacement_; }

    // Called when the Replace window opens. Forgets the remembered range and takes the document's
    // selection, when there is one, as the new range. The scope becomes SelectionOnly when the
    // selection spans more than one line, otherwise WholeDocument (a selection on one line is
    // used as the search text by the window and is not a scope).
    void capture_selection();

    // True when "Selection only" can be chosen: a range is remembered and usable, or the
    // document has a selection right now.
    bool selection_available() const;

    // The user's choice. WholeDocument always works. SelectionOnly takes the document's current
    // selection as the range when there is one; otherwise it keeps a usable remembered range;
    // otherwise it does nothing and returns false. Returns true when the scope was set.
    bool choose_scope(ReplaceScope scope);

    // The scope Replace All will use. SelectionOnly turns into WholeDocument by itself when the
    // range is no longer usable (another buffer, or its text was deleted).
    ReplaceScope scope() const;

    // Applies the typed search text, then does what SearchSession::replace() does with the
    // replacement. Afterwards the status shows "Replaced 1 match" when that was the last match,
    // "Wrapped to the top" when the next match was found past the end of the text, and otherwise
    // the usual count and position. Without a buffer nothing happens. `refused` is set when the
    // pattern or the replacement is not valid; status() then says why. While a job runs it does
    // nothing and returns an empty result.
    ReplaceResult replace();

    // Starts Replace All as a job. Busy, and nothing else, when a job runs. Otherwise it applies
    // the typed search text and then, in this order: NothingToReplace without a buffer; Refused
    // when the pattern or the replacement is not valid; it takes the range (the whole document, or
    // the remembered range for the SelectionOnly scope) and opens the session's run, which when it
    // is not started (the search is not active, or the range is empty) gives NothingToReplace with
    // the status "Nothing to replace". On Started it remembers the cursor, freezes the search
    // controller's status line and starts the job timer with the gap; the first slice runs when it
    // fires. The "of about" count of the progress is the search's match count of the whole
    // document when it is known at the start, and is left out for the SelectionOnly scope.
    ReplaceStart start_replace_all();

    // True from a Started start_replace_all() until the job ends.
    bool job_active() const { return job_.has_value(); }

    // Ends the running job as Cancelled: what was replaced stays, as one undo step, the status
    // shows replace_stopped_message(), and the done callback runs. Does nothing without a job.
    void cancel_job();

    // The matches replaced so far by the running job; 0 without a job.
    std::uint64_t job_replaced() const;

    // How a job paces itself. Takes effect at the next slice.
    void set_pacing(const ReplacePacing& pacing) { pacing_ = pacing; }
    const ReplacePacing& pacing() const { return pacing_; }

    // Called after every slice that did not end the job (status() shows the progress). Replaces
    // the previous callback.
    void set_on_job_progress(std::function<void()> callback);

    // Called once when a job ends, however it ends, after job_active() became false. Not called
    // when the controller is destroyed. Replaces the previous callback.
    void set_on_job_done(std::function<void(const ReplaceJobResult&)> callback);

    // The status line of the Replace window: the search controller's status, except that the
    // problem of the replacement text (replacement_problem_message() of replacement_error()) is
    // shown whenever there is a buffer, the search pattern is valid and the replacement is not,
    // so it appears while the user types, not only at the click. A problem of the pattern itself
    // comes first. While a job runs it is replacing_message() of the job's progress.
    std::string status() const;

    // True when status() is the problem text of the pattern or of the replacement; never while a
    // job runs.
    bool status_is_problem() const;

private:
    // The state of a running job.
    struct Job {
        // The controller's session, borrowed; alive until the buffer changes, and the controller's
        // buffer hook ends the job first.
        SearchSession* session = nullptr;
        std::uint64_t replaced = 0;
        // The match count of the whole document when the job began; 0 when unknown or not used.
        std::uint64_t total = 0;
        bool in_range = false;
        int range_characters = 0; // the length of the range, when in_range
        int characters = 0;       // the length of the whole text when the job began
        unsigned slices = 0;
        std::int64_t started_us = 0; // g_get_monotonic_time() at the start
        std::int64_t work_us = 0;    // the time spent inside slices
        std::int64_t rss_start_kb = 0;
        // Left gravity, at the insert mark when the job began, so that the cursor stays where it
        // was in the text after a whole-document job. A reference of our own (transfer none).
        gtkx::ObjectPtr<GtkTextMark> cursor;
    };

    GtkTextBuffer* text_buffer() const;
    // Why the replacement cannot be used, in words, or nothing.
    std::optional<std::string> replacement_problem() const;
    // Remembers [start, end) as the range, replacing an older one.
    void remember_range(const GtkTextIter& start, const GtkTextIter& end);
    // Forgets the range and removes its marks from the buffer when they are still in it.
    void drop_range();
    // The usable range as character offsets: false when there is none, when it belongs to another
    // buffer than the controller's, or when it has become empty.
    bool range_offsets(int* from, int* to) const;

    // The job (replace_controller_job.cpp). Runs one slice, then schedules the next or ends the
    // job.
    void run_job_slice();
    // Starts the job timer with the gap; its call runs run_job_slice().
    void schedule_job_slice();
    // Ends the job: stops the timer, ends the session's run, restores the cursor (whole document),
    // logs, shows the final message, unfreezes the status and, when `notify` is true, runs the done
    // callback.
    void finish_job(ReplaceJobEnd end, bool notify);
    // Logs the one line that ends a job; never the pattern or the replacement text. Called by
    // finish_job() while job_ is still set.
    void log_job_end(ReplaceJobEnd end, std::uint64_t count) const;

    SearchController& search_;
    std::unique_ptr<DebounceTimer> timer_;
    std::string replacement_;
    ReplaceScope scope_ = ReplaceScope::WholeDocument;
    ReplacePacing pacing_;
    std::optional<Job> job_;
    std::function<void()> on_job_progress_;
    std::function<void(const ReplaceJobResult&)> on_job_done_;
    // The range's marks: start with left gravity, end with right gravity. References of our own
    // (the buffer owns the marks only until they are deleted).
    gtkx::ObjectPtr<GtkTextMark> range_start_;
    gtkx::ObjectPtr<GtkTextMark> range_end_;
};

} // namespace vermilion::ui
