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

#include "core/recovered_base.h"
#include "core/text_format.h"
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

    GtkWidget* widget() const { return scrolled_.get(); } // the notebook page
    GtkWidget* tab() const { return label_->widget(); }
    GtkWidget* view() const { return GTK_WIDGET(view_); }

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
    // page's file is. The page cannot be edited until the save ends. When expect_file_time() was
    // called for path and overwrite_changes is false, the file's time is read first; if it differs
    // the save ends with ChangedOnDisk and writes nothing (the caller asks Save Anyway).
    void save(const std::string& path, const TextFormat& format, bool overwrite_changes);

    // True from the call of save() until its end, also while the file's time is being read.
    bool saving() const { return saver_ != nullptr || checking_; }
    bool busy() const { return loading() || saving(); }

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
    // the Unix epoch). See save(). Forgotten after a successful save, and by load().
    void expect_file_time(const std::string& path, std::int64_t mtime_us);

    void set_wrap(bool wrap);
    bool wraps() const;

    // Line, column, mode, tab width and language; the format fields are left to the caller.
    CursorStatus status() const;

    // The whole text as the buffer holds it: UTF-8, a byte-order mark as U+FEFF.
    std::string text() const;

    // The length of the text in characters (the buffer keeps the count, so this is cheap).
    std::uint64_t char_count() const;

    // Reads the modified time of the page's file as it is now, without blocking. callback gets
    // microseconds since the Unix epoch, or nothing when the page has no file or the time cannot
    // be read. It runs from the main loop (at once when the page has no file) and never after the
    // page is destroyed.
    void query_file_time(std::function<void(std::optional<std::int64_t>)> callback);

private:
    void on_loaded(DocumentLoader::Outcome outcome, const std::string& error);
    void on_saved(DocumentSaver::Outcome outcome, const std::string& error);
    void begin_save(const std::string& path, const TextFormat& format, bool overwrite_changes);
    void end_save(DocumentSaver::Outcome outcome, const std::string& error);
    void query_time(GFile* location, std::function<void(std::optional<std::int64_t>)> callback);

    Callbacks callbacks_;
    gtkx::ObjectPtr<GtkWidget> scrolled_; // the notebook holds its own reference as well
    GtkSourceView* view_ = nullptr;       // borrowed; owned by scrolled_
    GtkSourceBuffer* buffer_ = nullptr;   // borrowed; the view holds its reference
    gtkx::ObjectPtr<GtkSourceFile> file_; // files only; also used for saving
    std::unique_ptr<TabLabel> label_;
    std::unique_ptr<DocumentLoader> loader_; // set while the file loads
    std::unique_ptr<DocumentSaver> saver_;   // set while the text is saved
    gtkx::SignalConnection modified_changed_;
    gtkx::SignalConnection cursor_moved_;
    gtkx::SignalConnection overwrite_toggled_;
    gtkx::SignalConnection text_changed_;
    // load() is emptying the buffer, or restore_text() is filling it: that is not an edit.
    bool clearing_ = false;
    // Set by expect_file_time(), cleared by a successful save and by load().
    std::optional<RecoveredBase> base_;
    // save() is reading the file's time before it starts the saver.
    bool checking_ = false;
    gtkx::AsyncGuard<EditorPage> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
