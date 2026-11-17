// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// A search over one text buffer: options, count and position, Find Next and Previous, Replace.

#pragma once

#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "core/search_options.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// What a Find Next or a Find Previous did.
struct FindResult {
    bool found = false;
    // True only when a match was found and the search had to run past the end of the text (Find
    // Next) or past its start (Find Previous) to find it. Never true when nothing was found.
    bool wrapped = false;
};

// What Replace did.
struct ReplaceResult {
    // True when the pattern or the replacement text is not valid (SearchSession::error(), or
    // replacement_error() of core/search_replacement.h): nothing was done.
    bool refused = false;
    // True when the selection was a match and it was replaced.
    bool replaced = false;
    // The match selected afterwards: the next one after the replacement, or, when nothing was
    // replaced, the next one after the selection.
    FindResult next;
};

// What a Replace All run did (SearchSession::end_replace_run).
struct ReplaceAllResult {
    // The number of matches replaced by all the slices of the run; 0 for a run that found nothing.
    std::uint64_t count = 0;
};

// What one slice of a Replace All run did (SearchSession::replace_run_slice).
struct ReplaceSlice {
    // The matches this slice replaced.
    std::uint64_t replaced = 0;
    // True when no match is left to replace: the run only has to be ended.
    bool finished = false;
};

// The search that the Find and Replace windows run on one document's buffer. It owns the
// library's GtkSourceSearchSettings and GtkSourceSearchContext (the context counts the matches and
// highlights them in every view of the buffer), creates no widget and includes no window code, so
// it runs and is tested without a display. Everything runs on the main loop. The windows make one
// session per buffer; scrolling the view to a match is theirs (the session only selects it).
//
// While the search is not active (empty text, or a regular expression that does not compile) the
// library is handed an empty search text: it highlights nothing and finds nothing.
class SearchSession {
public:
    // The buffer is borrowed: the session takes a reference of its own and keeps the buffer alive
    // for as long as it lives. The search starts switched off (empty text).
    explicit SearchSession(GtkSourceBuffer* buffer);
    // Ends a Replace All run that is still active exactly as end_replace_run() does.
    ~SearchSession();

    SearchSession(const SearchSession&) = delete;
    SearchSession& operator=(const SearchSession&) = delete;

    // Replaces the options and starts a new scan of the buffer. Wrap-around is always on.
    void set_options(const SearchOptions& options);

    const SearchOptions& options() const { return options_; }

    // pattern_error() of the current options: why the regular expression does not compile.
    const std::optional<std::string>& error() const { return error_; }

    // True when the library has something to look for: the text is not empty and the pattern, when
    // there is one, compiles.
    bool active() const { return search_enabled(options_) && !error_.has_value(); }

    // True while the buffer is still being scanned for the current options (the count is unknown).
    bool searching() const;

    // The number of matches: 0 when the search is not active, negative while searching().
    int count() const;

    // The number of the match that is exactly the current selection, counted from 1; 0 when the
    // search is not active or the selection is not a match (also with no selection); negative
    // while the position is still unknown.
    int position() const;

    // Selects the next match, searching from the END of the current selection (from the cursor
    // when nothing is selected) and wrapping past the end of the text. The cursor is left at the
    // end of the match. When there is no match, or the search is not active, nothing changes.
    // Works at once, before the scan has finished.
    FindResult find_next();

    // Selects the previous match, searching from the START of the current selection and wrapping
    // past the start of the text. The cursor is left at the start of the match. Otherwise like
    // find_next().
    FindResult find_previous();

    // If the current selection is exactly a match, replaces it with `replacement` (one undo step),
    // puts the cursor after the replacement and selects the next match as find_next() does;
    // otherwise only selects the next match. The replacement is literal when regex is off; with
    // regex on GLib's replacement syntax applies (\0 to \9, \n, ...). Nothing happens, and
    // `refused` is set, when error() has a value or replacement_error(options(), replacement) has
    // one. With the search not active (empty text) the result is empty and nothing changes.
    ReplaceResult replace(const std::string& replacement);

    // Replace All in slices, so that the window stays alive while a very large text is worked on
    // (Adam, 2026-11-16: the library's own replace_all froze the window on a 50 MB file). A run
    // replaces the matches one by one, through the library's replace call, in text order; it stops
    // after every slice and is carried on by the caller from the main loop. Whole document:
    // from 0 to the number of characters of the text. While a run is active the caller must not
    // edit the buffer, change the options or use any other function of this session that changes
    // the text or the selection (the Replace window locks the document); the selection is left
    // alone until the run ends.
    //
    // Starts a run over the characters from offset `from` up to offset `to` (offsets count
    // characters; both are clamped to the text). Opens ONE undo step that stays open until
    // end_replace_run(), however many slices and main-loop turns lie between. Returns false and
    // starts nothing when the run is refused: error() or replacement_error() has a value, the
    // search is not active, the clamped range is empty (from >= to), or a run is already active.
    bool begin_replace_run(const std::string& replacement, int from, int to);

    // True from a successful begin_replace_run() until end_replace_run() (or the destructor).
    bool replace_run_active() const;

    // Replaces matches that lie wholly inside the range, in text order, and returns when one of
    // three things has happened: `max_matches` matches were replaced in this call (0 means no
    // limit); `deadline_us`, a time on the g_get_monotonic_time() clock in microseconds (0 means
    // none), has passed at one of the checks, which happen after every 16th match replaced in
    // this call; or no match is left, which sets `finished`. A slice that stops at a limit leaves
    // `finished` false even when it replaced the last match: the next slice then replaces 0
    // matches and finishes. Empty matches are skipped. Without an active run it replaces nothing
    // and reports finished.
    ReplaceSlice replace_run_slice(std::uint64_t max_matches, std::int64_t deadline_us);

    // Ends the run: closes its undo step (the whole run is one Undo), and, when at least one match
    // was replaced, leaves the range, which now ends after the last replacement, selected with
    // the cursor at its end. Returns the number of matches replaced by all the slices. Without an
    // active run it does nothing and returns a count of 0.
    ReplaceAllResult end_replace_run();

    // Called on the main loop whenever the library reports a new count: a scan has finished, the
    // search was switched off, or an edit of the buffer changed the matches. It may also be called
    // from inside set_options(). Replaces the previous callback; an empty function removes it.
    void set_on_count_changed(std::function<void()> callback);

private:
    FindResult find(bool forward);
    bool refuses(const std::string& replacement) const;
    void count_changed();

    gtkx::ObjectPtr<GtkSourceBuffer> buffer_;
    gtkx::ObjectPtr<GtkSourceSearchSettings> settings_;
    gtkx::ObjectPtr<GtkSourceSearchContext> context_;
    gtkx::SignalConnection count_connection_;
    SearchOptions options_;
    std::optional<std::string> error_;
    std::function<void()> on_count_changed_;
    struct Run; // the state of an active Replace All run; defined in the .cpp
    std::unique_ptr<Run> run_;
};

} // namespace vermilion::ui
