// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The one search that the Find and Replace windows share: typed text, options, status, Find Next.

#pragma once

#include <gtksourceview/gtksource.h>

#include <functional>
#include <memory>
#include <string>

#include "core/search_options.h"
#include "ui/debounce_timer.h"
#include "ui/search_session.h"

namespace vermilion::ui {

// Everything the Find window does that is not drawing, so that it is tested without a display:
// the text and the three checkboxes, the 200 ms pause before typed text is searched for, the
// search session on the buffer of the active document, and the text of the status line. The
// Replace window uses the same controller (one search for both windows, Adam, 2026-11-15).
// Creates no widget and includes no window code. Everything runs on the main loop. Scrolling the
// view to a match and showing the status are the windows' work: they call find_next() and read
// status().
//
// There are two sets of options. The typed ones are what the windows hold (the text may have been
// typed a moment ago); the applied ones are what the session searches for. They differ only while
// typed text waits for the timer (pending()).
class SearchController {
public:
    // How long typing must pause before the typed text is searched for (Adam, 2026-11-13).
    static constexpr unsigned kDebounceMs = 200;

    // The controller owns the timer. It starts with no buffer and empty options.
    explicit SearchController(std::unique_ptr<DebounceTimer> timer);

    SearchController(const SearchController&) = delete;
    SearchController& operator=(const SearchController&) = delete;

    // Makes `buffer` (borrowed; nullptr when no document is open) the text that is searched. The
    // same buffer again changes nothing. Another buffer: the old session is destroyed first (its
    // highlighting goes with it), a new one starts with the applied options, and a message shown
    // by show_message() or by a wrapped search is dropped.
    void set_buffer(GtkSourceBuffer* buffer);
    bool has_buffer() const { return session_ != nullptr; }

    // The buffer that is searched (borrowed), or nullptr when there is none.
    GtkSourceBuffer* buffer() const { return buffer_; }

    // Called on the main loop at the start of set_buffer() when the buffer really changes (also
    // to or from none), before the old session is destroyed, so that something that works on the
    // session (a Replace All job) can end first. Replaces the previous callback; an empty
    // function removes it.
    void set_on_buffer_changing(std::function<void()> callback);

    // The session on the current buffer (borrowed), or nullptr without a buffer. The Replace
    // window calls its replace functions with the replacement it holds.
    SearchSession* session() const { return session_.get(); }

    // The text as typed. Remembers it and starts the timer again (every keystroke restarts the
    // 200 ms); when the text is the applied text there is nothing to wait for: the timer is
    // stopped and pending() is false.
    void set_text(const std::string& text);
    const std::string& text() const { return typed_.text; }
    bool has_text() const { return !typed_.text.empty(); }

    // True from a set_text() that changed the text until the typed text is applied.
    bool pending() const { return pending_; }

    // Applies the typed text now (Enter, or the timer): stops the timer, clears pending() and, when
    // the options differ from the applied ones, gives them to the session, which starts a new
    // scan. When nothing differs the session is left alone, so the count is not lost.
    void apply();

    // The checkboxes. Each one applies at once, together with the typed text. Setting the value
    // that is already set does nothing.
    void set_match_case(bool match_case);
    void set_whole_word(bool whole_word);
    void set_regex(bool regex);

    // The applied options: the text that is being searched for and the three flags.
    const SearchOptions& options() const { return applied_; }

    // Applies the typed text, then Find Next or Find Previous on the session (nothing found
    // without a buffer). The message of an earlier wrapped search is dropped; a search that
    // wrapped sets wrapped_message() until the next find, option change, buffer change or
    // show_message().
    FindResult find_next();
    FindResult find_previous();

    // The status line, recomputed by refresh_status(). In order: a message (show_message() or a
    // wrapped search); "" without a buffer or when the applied text is empty; the problem text
    // pattern_problem_message() of the session's error; otherwise match_count_message() of the
    // session's position and count.
    const std::string& status() const { return status_; }

    // True when status() is the problem text of a pattern that does not compile.
    bool status_is_problem() const { return problem_; }

    // Shows `message` (for example replaced_message() after Replace All) in place of the count
    // until the next find, option change or buffer change; an empty message removes it.
    void show_message(const std::string& message);

    // Works the status out again (the count and position change without a call here: a scan
    // ends, the text is edited, the selection moves). Called by every function above that can
    // change it and whenever the session reports a new count; the windows call it when they come
    // forward. The status callback runs only when status() or status_is_problem() changed.
    void refresh_status();

    // Called on the main loop whenever status() or status_is_problem() changed. Replaces the
    // previous callback; an empty function removes it.
    void set_on_status_changed(std::function<void()> callback);

    // While frozen, refresh_status() does nothing: the status keeps its text and no callback runs
    // (a Replace All job freezes it, because the match count changes with every edit). Unfreezing
    // refreshes at once. Freezing twice, or unfreezing when not frozen, does nothing more.
    void set_status_frozen(bool frozen);

private:
    // Makes applied_ equal to typed_. When that changes it: drops the message, gives the options
    // to the session (when there is one) and refreshes the status. Never touches the timer.
    void apply_options();

    // Find Next (forward) or Find Previous: applies the typed text, searches, sets or drops the
    // wrapped message and refreshes the status.
    FindResult find(bool forward);

    std::unique_ptr<DebounceTimer> timer_;
    std::unique_ptr<SearchSession> session_;
    // Borrowed; the session keeps the buffer alive. Null exactly when there is no session.
    GtkSourceBuffer* buffer_ = nullptr;
    SearchOptions typed_;
    SearchOptions applied_;
    bool pending_ = false;
    std::string message_; // the message that replaces the count; empty when none
    std::string status_;
    bool problem_ = false;
    std::function<void()> on_status_changed_;
    std::function<void()> on_buffer_changing_;
    bool frozen_ = false;
};

} // namespace vermilion::ui
