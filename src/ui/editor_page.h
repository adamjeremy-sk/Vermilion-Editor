// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// One document's editor page: buffer, view, tab label, and for a file its source file, load, save.

#pragma once

#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "core/baseline.h"
#include "core/held_events.h"
#include "core/text_format.h"
#include "ui/change_banner.h"
#include "ui/document_loader.h"
#include "ui/document_saver.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/status_bar.h"
#include "ui/tab_label.h"

namespace vermilion::ui {

class EditorPage {
public:
    // While a file loads, the buffer's signals are not passed on: the inserted text is not an
    // edit, and measuring the cursor column after every inserted block of a very long line would
    // make the load slow. While it saves, the page cannot be edited.
    struct Callbacks {
        std::function<void()> close;            // the tab's close button or a middle click
        std::function<void(int)> scroll_tabs;   // mouse wheel over the tab: +1 or -1
        std::function<void(bool)> modified;     // the buffer's modified flag changed
        std::function<void()> cursor;           // the cursor moved or the mode was toggled
        std::function<void(int)> load_progress; // percent loaded, only when it changes
        // The load ended; runs last and may destroy this page. The error text when Failed.
        std::function<void(DocumentLoader::Outcome, const std::string&)> loaded;
        std::function<void(int)> save_progress; // percent saved, only when it changes
        // The save ended; runs last and may destroy this page. The error text when Failed.
        std::function<void(DocumentSaver::Outcome, const std::string&)> saved;
        // The text changed, or its format was marked as changed (mark_modified); not while a file
        // loads.
        std::function<void()> edited;
    };

    // An empty, editable page with the classic scheme, line numbers and no word wrap. The text is
    // kept exactly as in the file: no newline is removed at the end on load or added on save.
    explicit EditorPage(Callbacks callbacks);

    EditorPage(const EditorPage&) = delete;
    EditorPage& operator=(const EditorPage&) = delete;

    // The notebook page: a vertical box with the change banner (hidden) above the scrolled view.
    GtkWidget* widget() const { return root_.get(); }
    GtkWidget* tab() const { return label_->widget(); }
    GtkWidget* view() const { return GTK_WIDGET(view_); }
    // The document's text buffer (borrowed); the search runs on it.
    GtkSourceBuffer* buffer() const { return buffer_; }

    // The strip above the text (ui/change_banner.h). show_banner() shows text with two buttons and
    // replaces a banner that is already shown, together with its two functions. A click runs
    // on_primary or on_secondary (a copy, so the function may call hide_banner()); the banner stays
    // until hide_banner(). load() hides it. The functions must not destroy the page.
    void show_banner(const std::string& text, const std::string& primary_label,
                     const std::string& secondary_label, std::function<void()> on_primary,
                     std::function<void()> on_secondary);
    void hide_banner();
    bool banner_visible() const;
    // The text of the banner that is shown; empty when none.
    const std::string& banner_text() const;

    void set_label(const std::string& title, const std::string& tooltip, bool modified);
    bool contains_close_button(GtkWidget* widget) const;

    // Starts loading the file at path (absolute) with exactly this encoding, replacing the text
    // (also for Reopen as). The page cannot be edited, and the text is outside the undo history,
    // until the load ends.
    void load(const std::string& path, TextEncoding encoding);
    bool loading() const { return loader_ != nullptr; }

    // After a successful load: editable unless read_only, the language guessed from path, the
    // cursor at the start and the buffer unmodified.
    void finish_load(const std::string& path, bool read_only);

    // Starts saving the text to path (absolute) with format; Save As when path is not where the
    // page's file is. The page cannot be edited until the save ends. Unless overwrite_changes is
    // true, a save to the page's own file first compares the file's time and size with the page's
    // baseline (core/baseline.h); if they differ the save ends with ChangedOnDisk and writes
    // nothing (the caller asks Save Anyway). GtkSourceView's own check is never used: the saver
    // always gets its ignore flag.
    void save(const std::string& path, const TextFormat& format, bool overwrite_changes);

    // True from the call of save() until its end, also while the file's time is being read.
    bool saving() const { return saver_ != nullptr || checking_; }

    // Locks the page for a job that edits the buffer in slices (Replace All, Adam 2026-11-16): the
    // view cannot be edited (a read-only page stays read-only after the unlock), busy() is true,
    // and the buffer's edit and cursor notifications are held back and passed on once at the
    // unlock. Locking a page that loads or saves, locking a locked page and unlocking a page that
    // is not locked do nothing.
    void set_locked(bool locked);
    bool locked() const { return held_.holding(); }

    // True while the page loads, saves or is locked: the commands that write it are off.
    bool busy() const { return loading() || saving() || locked(); }

    // Sets the language (syntax colouring) from the file name; plain text when nothing matches.
    void guess_language(const std::string& path);

    // The text starts with U+FEFF, the byte-order mark as the text holds it.
    bool starts_with_bom() const;

    // Adds or removes the U+FEFF at the start, as one edit that Undo takes back.
    void set_bom(bool bom);

    // Marks the text as changed, so a new format reaches the file with the next save.
    void mark_modified();

    // Replaces the text with the text of a recovery copy (UTF-8 without NUL; at most INT_MAX
    // bytes): one insert, outside the undo history, with the cursor at the start and the buffer
    // modified. It is not passed on as an edit, because the copy it came from still exists.
    void restore_text(const std::string& text);

    // The text is based on the file at path (absolute) as it was at mtime_us (microseconds since
    // the Unix epoch): a time-only baseline, which a recovered document has until its first save.
    // See save(). Replaced by sync_baseline() after a successful save, and dropped by load().
    void expect_file_time(const std::string& path, std::int64_t mtime_us);

    // Where the user is looking: the cursor's place and how far the view is scrolled. A reload
    // from disk keeps it.
    struct ViewPosition {
        int line = 0;        // 0-based line of the cursor
        int line_offset = 0; // 0-based character offset of the cursor in its line
        double scroll = 0.0; // the vertical scroll position, in pixels
    };

    ViewPosition view_position() const;

    // Puts the cursor back at the saved line and column, clamped to the text as it is now, and the
    // scroll position back: at once and again from an idle callback, because GTK works out the
    // height of a long text a little at a time. Does nothing while the page loads. Call it after
    // finish_load().
    void restore_view_position(const ViewPosition& position);

    // True when the page's text and baseline can be trusted: it does not load, save or read its
    // baseline, and is not locked. A look at the file waits until this is true.
    bool quiet() const { return !busy() && !syncing_; }

    // The page's baseline (core/baseline.h), when it has one.
    const std::optional<Baseline>& baseline() const { return baseline_; }

    // Makes this stamp the baseline; its path stays. Does nothing for a page without a baseline.
    void set_baseline_stamp(const FileStamp& stamp);

    void set_wrap(bool wrap);
    bool wraps() const;

    // Line, column, mode, tab width and language; the format fields are left to the caller.
    CursorStatus status() const;

    // The whole text as the buffer holds it: UTF-8, a byte-order mark as U+FEFF.
    std::string text() const;

    // The length of the text in characters (the buffer keeps the count, so this is cheap).
    std::uint64_t char_count() const;

    // Makes the page's baseline the file's state as it is now. Call it when a load or a save has
    // just made the text and the file equal. The file's time and size are read and its bytes are
    // hashed off the main loop, kept as the baseline, and callback gets the file's time
    // (microseconds since the Unix epoch), or nothing when the page has no local file or its state
    // cannot be read (the baseline is dropped then). While this runs (syncing_) a save skips the
    // baseline check. The callback runs from the main loop, never after the page is destroyed.
    void sync_baseline(std::function<void(std::optional<std::int64_t>)> callback);

private:
    void on_loaded(DocumentLoader::Outcome outcome, const std::string& error);
    void on_saved(DocumentSaver::Outcome outcome, const std::string& error);
    void begin_save(const std::string& path, const TextFormat& format);
    void end_save(DocumentSaver::Outcome outcome, const std::string& error);
    void apply_scroll(double value);
    static gboolean on_restore_scroll(gpointer ticket);

    Callbacks callbacks_;
    gtkx::ObjectPtr<GtkWidget> root_;     // the notebook page; the notebook holds a reference too
    gtkx::ObjectPtr<GtkWidget> scrolled_; // the text view's scrolled window, a child of root_
    GtkSourceView* view_ = nullptr;       // borrowed; owned by scrolled_
    GtkSourceBuffer* buffer_ = nullptr;   // borrowed; the view holds its reference
    gtkx::ObjectPtr<GtkSourceFile> file_; // files only; also used for saving
    std::unique_ptr<TabLabel> label_;
    std::unique_ptr<ChangeBanner> banner_;
    std::function<void()> banner_primary_; // what the banner's buttons do, set by show_banner()
    std::function<void()> banner_secondary_;
    std::unique_ptr<DocumentLoader> loader_; // set while the file loads
    std::unique_ptr<DocumentSaver> saver_;   // set while the text is saved
    gtkx::SignalConnection modified_changed_;
    gtkx::SignalConnection cursor_moved_;
    gtkx::SignalConnection overwrite_toggled_;
    gtkx::SignalConnection text_changed_;
    gtkx::SignalConnection lock_keys_; // stops the Undo and Redo keys while the page is locked
    // load() is emptying the buffer, or restore_text() is filling it: that is not an edit.
    bool clearing_ = false;
    // The file's path and stamp as they were when the text and the file were last equal; set by
    // sync_baseline() and expect_file_time(), dropped by load() and by a successful save.
    std::optional<Baseline> baseline_;
    // save() is reading the file's time before it starts the saver.
    bool checking_ = false;
    // sync_baseline() is reading the file.
    bool syncing_ = false;
    // set_locked(): the notifications held back while a job edits the buffer, and whether the
    // view was editable before the lock.
    HeldEvents held_;
    bool editable_before_lock_ = true;
    double pending_scroll_ = 0.0; // the position restore_view_position() is putting back
    gtkx::AsyncGuard<EditorPage> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
