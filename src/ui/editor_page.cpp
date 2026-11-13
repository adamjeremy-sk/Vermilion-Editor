// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds a document's editor, loads and saves its file, and reports its edits, cursor and I/O.

#include "ui/editor_page.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr unsigned kTabWidth = 4; // per document; the settings value arrives with M2.5
constexpr gunichar kByteOrderMark = 0xFEFF;
constexpr const char* kFileTimeAttributes =
    G_FILE_ATTRIBUTE_TIME_MODIFIED "," G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC;

// A file time query on its way: what to call with the answer, and the ticket that says whether
// the page is still there.
struct FileTimeQuery {
    std::function<void(std::optional<std::int64_t>)> callback;
    gpointer ticket = nullptr;
};

// The end of a query started by query_file_time(); data is the FileTimeQuery it released.
void on_file_time(GObject* source, GAsyncResult* result, gpointer data) {
    std::unique_ptr<FileTimeQuery> query(static_cast<FileTimeQuery*>(data));
    GError* raw = nullptr;
    auto info =
        gtkx::ObjectPtr<GFileInfo>::adopt(g_file_query_info_finish(G_FILE(source), result, &raw));
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    if (gtkx::AsyncGuard<EditorPage>::take(query->ticket) == nullptr) {
        return;
    }
    std::optional<std::int64_t> mtime_us;
    if (info) {
        std::unique_ptr<GDateTime, decltype(&g_date_time_unref)> time(
            g_file_info_get_modification_date_time(info.get()), &g_date_time_unref);
        if (time) {
            mtime_us = g_date_time_to_unix_usec(time.get());
        }
    }
    query->callback(mtime_us);
}

} // namespace

// The scrolled window is floating from _new; adopt_floating sinks it into scrolled_, and it sinks
// the (floating) view. The buffer's full reference from _new drops at the end of the constructor,
// after the view has taken its own.
EditorPage::EditorPage(Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      scrolled_(gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_scrolled_window_new())) {
    auto buffer = gtkx::ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    gtk_source_buffer_set_implicit_trailing_newline(buffer.get(), FALSE);
    GtkSourceStyleScheme* scheme = gtk_source_style_scheme_manager_get_scheme(
        gtk_source_style_scheme_manager_get_default(), "classic"); // borrowed
    if (scheme != nullptr) {
        gtk_source_buffer_set_style_scheme(buffer.get(), scheme);
    }
    GtkWidget* view = gtk_source_view_new_with_buffer(buffer.get());
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(view), kTabWidth);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_widget_add_css_class(scrolled_.get(), "vm-editor-well");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_.get()), view);
    view_ = GTK_SOURCE_VIEW(view);
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
        if (!loading() && !clearing_) {
            callbacks_.edited();
        }
    });
    cursor_moved_ = gtkx::connect<void(GObject*, GParamSpec*)>(buffer_, "notify::cursor-position",
                                                               [this](GObject*, GParamSpec*) {
                                                                   if (!loading()) {
                                                                       callbacks_.cursor();
                                                                   }
                                                               });
    overwrite_toggled_ = gtkx::connect<void(GObject*, GParamSpec*)>(view_, "notify::overwrite",
                                                                    [this](GObject*, GParamSpec*) {
                                                                        if (!loading()) {
                                                                            callbacks_.cursor();
                                                                        }
                                                                    });
}

void EditorPage::set_label(const std::string& title, const std::string& tooltip, bool modified) {
    label_->set(title, tooltip, modified);
}

bool EditorPage::contains_close_button(GtkWidget* widget) const {
    return label_->contains_close_button(widget);
}

// Full references from _new (not floating); the source file stays with the page for saving.
void EditorPage::load(const std::string& path, TextEncoding encoding) {
    file_ = gtkx::ObjectPtr<GtkSourceFile>::adopt(gtk_source_file_new());
    auto location = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    gtk_source_file_set_location(file_.get(), location.get());
    // The file is read again, so GtkSourceView knows its time now: nothing to check at a save.
    base_.reset();
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

// A page rebuilt from a recovery copy has no base time for GtkSourceView's own check, so its first
// save to the copy's file reads the file's time itself and ends like the saver does, with
// ChangedOnDisk and nothing written, when the time is not the recorded one. A file that is gone
// is not a change: the save writes it again.
void EditorPage::save(const std::string& path, const TextFormat& format, bool overwrite_changes) {
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view_), FALSE);
    if (!needs_base_check(base_, path, overwrite_changes)) {
        begin_save(path, format, overwrite_changes);
        return;
    }
    checking_ = true;
    auto location = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    query_time(location.get(), [this, path, format](std::optional<std::int64_t> current_us) {
        checking_ = false;
        if (base_ && file_changed_since_base(*base_, current_us)) {
            end_save(DocumentSaver::Outcome::ChangedOnDisk, std::string());
            return;
        }
        begin_save(path, format, false);
    });
}

// An untitled page gets its source file here; the saver gives it the location after the save.
void EditorPage::begin_save(const std::string& path, const TextFormat& format,
                            bool overwrite_changes) {
    if (!file_) {
        file_ = gtkx::ObjectPtr<GtkSourceFile>::adopt(gtk_source_file_new());
    }
    auto target = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    saver_ = std::make_unique<DocumentSaver>(
        buffer_, file_.get(), target.get(), format, overwrite_changes,
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
    base_ = RecoveredBase{path, mtime_us};
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

void EditorPage::query_file_time(std::function<void(std::optional<std::int64_t>)> callback) {
    GFile* location = file_ ? gtk_source_file_get_location(file_.get()) : nullptr;
    if (location == nullptr) {
        callback(std::nullopt);
        return;
    }
    query_time(location, std::move(callback));
}

// The location is borrowed; GIO keeps its own reference while the query runs. The query is not
// cancelled when the page goes: the guard ticket makes its answer do nothing then.
void EditorPage::query_time(GFile* location,
                            std::function<void(std::optional<std::int64_t>)> callback) {
    auto query = std::make_unique<FileTimeQuery>();
    query->callback = std::move(callback);
    query->ticket = guard_.ticket();
    g_file_query_info_async(location, kFileTimeAttributes, G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT, nullptr, &on_file_time, query.release());
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
        base_.reset();
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
