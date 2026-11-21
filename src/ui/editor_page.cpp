// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds a document's editor, loads and saves its file, and reports its edits, cursor and I/O.

#include "ui/editor_page.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "ui/file_inspector.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr gunichar kByteOrderMark = 0xFEFF;

// The keys that run Undo or Redo in the text view: Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z.
bool is_undo_or_redo_key(guint keyval, GdkModifierType state) {
    if ((state & GDK_CONTROL_MASK) == 0 || (state & GDK_ALT_MASK) != 0) {
        return false;
    }
    return keyval == GDK_KEY_z || keyval == GDK_KEY_Z || keyval == GDK_KEY_y || keyval == GDK_KEY_Y;
}

} // namespace

// The scrolled window is floating from _new; adopt_floating sinks it into scrolled_, and it sinks
// the (floating) view. The buffer's full reference from _new drops at the end of the constructor,
// after the view has taken its own. The box is floating from _new too, and adopt_floating sinks it
// into root_; gtk_box_append below makes the box take its own reference to scrolled_ and to the
// banner.
EditorPage::EditorPage(Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      root_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0))),
      scrolled_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_scrolled_window_new())) {
    auto buffer = gtkx::ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    gtk_source_buffer_set_implicit_trailing_newline(buffer.get(), FALSE);
    GtkWidget* view = gtk_source_view_new_with_buffer(buffer.get());
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_widget_add_css_class(scrolled_.get(), "vm-editor-well");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_.get()), view);
    view_ = GTK_SOURCE_VIEW(view);
    auto run_primary = [this] {
        std::function<void()> run = banner_primary_;
        if (run) {
            run();
        }
    };
    auto run_secondary = [this] {
        std::function<void()> run = banner_secondary_;
        if (run) {
            run();
        }
    };
    banner_ = std::make_unique<ChangeBanner>(ChangeBanner::Callbacks{run_primary, run_secondary});
    gtk_widget_set_vexpand(scrolled_.get(), TRUE);
    gtk_box_append(GTK_BOX(root_.get()), banner_->widget());
    gtk_box_append(GTK_BOX(root_.get()), scrolled_.get());
    buffer_ = buffer.get();
    label_ = std::make_unique<TabLabel>(TabLabel::Callbacks{
        [this] { callbacks_.close(); }, [this](int step) { callbacks_.scroll_tabs(step); }});
    modified_changed_ = gtkx::connect<void(GtkTextBuffer*)>(
        buffer_, "modified-changed", [this](GtkTextBuffer* text) {
            if (!loading()) {
                callbacks_.modified(gtk_text_buffer_get_modified(text) != FALSE);
            }
        });
    text_changed_ = gtkx::connect<void(GtkTextBuffer*)>(buffer_, "changed", [this](GtkTextBuffer*) {
        if (!loading() && !clearing_ && !held_.hold_edited()) {
            callbacks_.edited();
        }
    });
    cursor_moved_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        buffer_, "notify::cursor-position", [this](GObject*, GParamSpec*) {
            if (!loading() && !held_.hold_cursor()) {
                callbacks_.cursor();
            }
        });
    overwrite_toggled_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        view_, "notify::overwrite", [this](GObject*, GParamSpec*) {
            if (!loading() && !held_.hold_cursor()) {
                callbacks_.cursor();
            }
        });
    // While a Replace All job runs (the page is locked) the view's own Ctrl+Z, Ctrl+Y and
    // Ctrl+Shift+Z must do nothing: the window's Undo and Redo actions are greyed out then, so
    // the keys fall through to the view's built-in bindings, which undid the job's edits while it
    // was still replacing (Adam's hands-on test, 2026-11-17). The controller belongs to the view;
    // it runs in the capture phase, before the view's own bindings.
    GtkEventController* lock_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(lock_keys, GTK_PHASE_CAPTURE);
    lock_keys_ = gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
        lock_keys, "key-pressed",
        [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType state) -> gboolean {
            return (held_.holding() && is_undo_or_redo_key(keyval, state)) ? TRUE : FALSE;
        });
    gtk_widget_add_controller(view, lock_keys);
    // The scheme, the band under the cursor's line, the tab and the font class: the default look,
    // until the tabs give this page the wanted one.
    apply_look(EditorLook{});
}

void EditorPage::show_banner(const std::string& text, const std::string& primary_label,
                             const std::string& secondary_label, std::function<void()> on_primary,
                             std::function<void()> on_secondary) {
    banner_primary_ = std::move(on_primary);
    banner_secondary_ = std::move(on_secondary);
    banner_->show(text, primary_label, secondary_label);
}

void EditorPage::hide_banner() {
    banner_->hide();
    banner_primary_ = nullptr;
    banner_secondary_ = nullptr;
}

bool EditorPage::banner_visible() const {
    return banner_->visible();
}

const std::string& EditorPage::banner_text() const {
    return banner_->text();
}

void EditorPage::set_label(const std::string& title, const std::string& tooltip, bool modified) {
    label_->set(title, tooltip, modified);
}

bool EditorPage::contains_close_button(GtkWidget* widget) const {
    return label_->contains_close_button(widget);
}

// Full references from _new (not floating); the source file stays with the page for saving.
void EditorPage::load(const std::string& path, TextEncoding encoding) {
    hide_banner();
    file_ = gtkx::ObjectPtr<GtkSourceFile>::adopt(gtk_source_file_new());
    auto location = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    gtk_source_file_set_location(file_.get(), location.get());
    // The file is read again; the baseline is recorded after the load (sync_baseline).
    baseline_.reset();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view_), FALSE);
    gtk_text_buffer_begin_irreversible_action(GTK_TEXT_BUFFER(buffer_));
    clearing_ = true;
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer_), "", 0);
    clearing_ = false;
    loader_ = std::make_unique<DocumentLoader>(
        buffer_, file_.get(), encoding,
        DocumentLoader::Callbacks{[this](int percent) { callbacks_.load_progress(percent); },
                                  [this](DocumentLoader::Outcome outcome,
                                         const std::string& error) { on_loaded(outcome, error); }});
}

// The language is set only now: setting it before the load makes large files load many times
// slower (ARCHITECTURE.md 7.1).
void EditorPage::finish_load(const std::string& path, bool read_only) {
    GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer_);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view_), read_only ? FALSE : TRUE);
    guess_language(path);
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(text, &start);
    gtk_text_buffer_place_cursor(text, &start);
    gtk_text_buffer_set_modified(text, FALSE);
}

// A page with a baseline checks the file itself, because the saver is always told to ignore
// GtkSourceView's own check: the file's time and size are read off the main loop and compared
// with the baseline, and the save ends with ChangedOnDisk and nothing written when they differ.
// A file that is gone is not a change (the save writes it again), and an unreadable file state
// is not a change either (the save then fails or succeeds on its own terms).
void EditorPage::save(const std::string& path, const TextFormat& format, bool overwrite_changes) {
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view_), FALSE);
    if (syncing_ || !needs_baseline_check(baseline_, path, overwrite_changes)) {
        begin_save(path, format);
        return;
    }
    checking_ = true;
    inspect_file(path, false,
                 [this, ticket = guard_.ticket(), path, format](const FileInspection& inspection) {
                     if (gtkx::AsyncGuard<EditorPage>::take(ticket) == nullptr) {
                         return;
                     }
                     checking_ = false;
                     if (baseline_ && inspection.readable &&
                         changed_since_baseline(*baseline_, inspection.disk)) {
                         end_save(DocumentSaver::Outcome::ChangedOnDisk, std::string());
                         return;
                     }
                     begin_save(path, format);
                 });
}

// An untitled page gets its source file here; the saver gives it the location after the save.
void EditorPage::begin_save(const std::string& path, const TextFormat& format) {
    if (!file_) {
        file_ = gtkx::ObjectPtr<GtkSourceFile>::adopt(gtk_source_file_new());
    }
    auto target = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    // GtkSourceView's own changed-on-disk check is never used: the page checks the baseline.
    saver_ = std::make_unique<DocumentSaver>(
        buffer_, file_.get(), target.get(), format, true,
        DocumentSaver::Callbacks{[this](int percent) { callbacks_.save_progress(percent); },
                                 [this](DocumentSaver::Outcome outcome, const std::string& error) {
                                     on_saved(outcome, error);
                                 }});
}

// The language object is borrowed; nullptr (no match) means plain text.
void EditorPage::guess_language(const std::string& path) {
    GtkSourceLanguage* language = gtk_source_language_manager_guess_language(
        gtk_source_language_manager_get_default(), path.c_str(), nullptr);
    gtk_source_buffer_set_language(buffer_, language);
}

bool EditorPage::starts_with_bom() const {
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(buffer_), &start);
    return gtk_text_iter_get_char(&start) == kByteOrderMark;
}

void EditorPage::set_bom(bool bom) {
    if (bom == starts_with_bom()) {
        return;
    }
    GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer_);
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(text, &start);
    gtk_text_buffer_begin_user_action(text);
    if (bom) {
        gtk_text_buffer_insert(text, &start, "\uFEFF", -1);
    } else {
        GtkTextIter after = start;
        gtk_text_iter_forward_char(&after);
        gtk_text_buffer_delete(text, &start, &after);
    }
    gtk_text_buffer_end_user_action(text);
}

void EditorPage::mark_modified() {
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(buffer_), TRUE);
    if (!loading()) {
        callbacks_.edited();
    }
}

// One insert: GtkSourceView's undo history stays empty, so Undo cannot take the text away. The
// insert is not an edit (clearing_), but the buffer's modified flag is set whatever the text.
void EditorPage::restore_text(const std::string& text) {
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_);
    clearing_ = true;
    gtk_text_buffer_begin_irreversible_action(buffer);
    gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<int>(text.size()));
    gtk_text_buffer_end_irreversible_action(buffer);
    clearing_ = false;
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);
    gtk_text_buffer_set_modified(buffer, TRUE);
}

void EditorPage::expect_file_time(const std::string& path, std::int64_t mtime_us) {
    FileStamp stamp;
    stamp.mtime_us = mtime_us;
    stamp.time_only = true;
    baseline_ = Baseline{path, stamp};
}

// The lock reuses the view's own editable flag, as save() does. The job edits the buffer through
// the API, which the flag does not stop, so the flag only stops what the user does in the view:
// typing, Cut, Paste and drag and drop. The Undo and Redo keys are stopped by `lock_keys_`.
void EditorPage::set_locked(bool locked) {
    if (locked == held_.holding()) {
        return;
    }
    GtkTextView* text_view = GTK_TEXT_VIEW(view_);
    if (locked) {
        if (busy()) {
            return;
        }
        editable_before_lock_ = gtk_text_view_get_editable(text_view) != FALSE;
        gtk_text_view_set_editable(text_view, FALSE);
        held_.hold();
        return;
    }
    const HeldEvents::Pending pending = held_.release();
    gtk_text_view_set_editable(text_view, editable_before_lock_ ? TRUE : FALSE);
    if (pending.edited && !loading()) {
        callbacks_.edited();
    }
    if (pending.cursor && !loading()) {
        callbacks_.cursor();
    }
}

void EditorPage::set_baseline_stamp(const FileStamp& stamp) {
    if (baseline_) {
        baseline_->stamp = stamp;
    }
}

// The cursor's line and column and the scroll position as they are now.
EditorPage::ViewPosition EditorPage::view_position() const {
    GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer_);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(text, &iter, gtk_text_buffer_get_insert(text));
    ViewPosition position;
    position.line = gtk_text_iter_get_line(&iter);
    position.line_offset = gtk_text_iter_get_line_offset(&iter);
    GtkAdjustment* adjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_.get())); // borrowed
    position.scroll = gtk_adjustment_get_value(adjustment);
    return position;
}

// After gtk_text_iter_forward_to_line_end `end` holds the end of the line (before its line
// ending), so its line offset is the longest column the cursor can have on that line.
void EditorPage::restore_view_position(const ViewPosition& position) {
    if (loading()) {
        return;
    }
    GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer_);
    const int last_line = gtk_text_buffer_get_line_count(text) - 1;
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(text, &iter, std::clamp(position.line, 0, last_line));
    GtkTextIter end = iter;
    if (!gtk_text_iter_ends_line(&end)) {
        gtk_text_iter_forward_to_line_end(&end);
    }
    const int longest = gtk_text_iter_get_line_offset(&end);
    gtk_text_iter_set_line_offset(&iter, std::clamp(position.line_offset, 0, longest));
    gtk_text_buffer_place_cursor(text, &iter);
    pending_scroll_ = position.scroll;
    apply_scroll(position.scroll);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, &EditorPage::on_restore_scroll, guard_.ticket(),
                    &gtkx::AsyncGuard<EditorPage>::free_ticket);
}

void EditorPage::apply_scroll(double value) {
    GtkAdjustment* adjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_.get())); // borrowed
    gtk_adjustment_set_value(adjustment, value);
}

gboolean EditorPage::on_restore_scroll(gpointer ticket) {
    EditorPage* self = gtkx::AsyncGuard<EditorPage>::peek(ticket);
    if (self != nullptr) {
        self->apply_scroll(self->pending_scroll_);
    }
    return G_SOURCE_REMOVE;
}

void EditorPage::set_wrap(bool wrap) {
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view_), wrap ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
}

bool EditorPage::wraps() const {
    return gtk_text_view_get_wrap_mode(GTK_TEXT_VIEW(view_)) != GTK_WRAP_NONE;
}

CursorStatus EditorPage::status() const {
    GtkTextBuffer* text = GTK_TEXT_BUFFER(buffer_);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(text, &iter, gtk_text_buffer_get_insert(text));
    CursorStatus status;
    status.line = gtk_text_iter_get_line(&iter) + 1;
    status.column = static_cast<int>(gtk_source_view_get_visual_column(view_, &iter)) + 1;
    status.overwrite = gtk_text_view_get_overwrite(GTK_TEXT_VIEW(view_)) != FALSE;
    status.tab_width = gtk_source_view_get_tab_width(view_);
    GtkSourceLanguage* language = gtk_source_buffer_get_language(buffer_); // borrowed
    if (language != nullptr) {
        status.language = gtkx::from_utf8(gtk_source_language_get_name(language));
    }
    return status;
}

std::string EditorPage::text() const {
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(buffer_);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtkx::take_string(gtk_text_buffer_get_text(buffer, &start, &end, TRUE));
}

// GtkTextBuffer counts characters in an int, so the count is never negative.
std::uint64_t EditorPage::char_count() const {
    int count = gtk_text_buffer_get_char_count(GTK_TEXT_BUFFER(buffer_));
    return count > 0 ? static_cast<std::uint64_t>(count) : 0;
}

// The location is borrowed; the inspector keeps what it needs while it runs. The inspection is
// not cancelled when the page goes: the guard ticket makes its answer do nothing then.
void EditorPage::sync_baseline(std::function<void(std::optional<std::int64_t>)> callback) {
    GFile* location = file_ ? gtk_source_file_get_location(file_.get()) : nullptr; // borrowed
    char* raw = location != nullptr ? g_file_get_path(location) : nullptr;
    if (raw == nullptr) {
        baseline_.reset();
        callback(std::nullopt);
        return;
    }
    std::string path(raw);
    g_free(raw);
    syncing_ = true;
    inspect_file(path, true,
                 [this, ticket = guard_.ticket(), path,
                  callback = std::move(callback)](const FileInspection& inspection) {
                     if (gtkx::AsyncGuard<EditorPage>::take(ticket) == nullptr) {
                         return;
                     }
                     syncing_ = false;
                     if (!inspection.readable || !inspection.disk.exists) {
                         baseline_.reset();
                         callback(std::nullopt);
                         return;
                     }
                     FileStamp stamp;
                     stamp.mtime_us = inspection.disk.mtime_us;
                     stamp.size = inspection.disk.size;
                     stamp.hash = inspection.hash;
                     baseline_ = Baseline{path, stamp};
                     callback(stamp.mtime_us);
                 });
}

// Runs from the loader's own callback, which allows destroying the loader here. The page's
// callback runs last, because it may destroy the page.
void EditorPage::on_loaded(DocumentLoader::Outcome outcome, const std::string& error) {
    gtk_text_buffer_end_irreversible_action(GTK_TEXT_BUFFER(buffer_));
    loader_.reset();
    std::function<void(DocumentLoader::Outcome, const std::string&)> loaded = callbacks_.loaded;
    loaded(outcome, error);
}

// Runs from the saver's own callback, which allows destroying the saver here.
void EditorPage::on_saved(DocumentSaver::Outcome outcome, const std::string& error) {
    saver_.reset();
    if (outcome == DocumentSaver::Outcome::Saved) {
        baseline_.reset();
    }
    end_save(outcome, error);
}

// Saving is never started for a read-only page, so the page is editable again afterwards. The
// page's callback runs last, because it may destroy the page.
void EditorPage::end_save(DocumentSaver::Outcome outcome, const std::string& error) {
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view_), TRUE);
    std::function<void(DocumentSaver::Outcome, const std::string&)> saved = callbacks_.saved;
    saved(outcome, error);
}

} // namespace vermilion::ui
