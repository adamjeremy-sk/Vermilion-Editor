// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The main window's part of search: the Find, Replace and Go to Line windows, and the search
// following the active document.

#include "ui/main_window.h"

#include <gtk/gtk.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/search_messages.h"
#include "ui/editor_page.h"
#include "ui/gtkx/signal.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

// A selection longer than this is never copied as the search text.
constexpr int kMaxSelectionChars = 256;

} // namespace

void MainWindow::sync_search_buffer() {
    EditorPage* editor = tabs_.active_editor(); // borrowed
    search_.set_buffer(editor != nullptr ? editor->buffer() : nullptr);
}

std::optional<std::string> MainWindow::selected_search_text() const {
    EditorPage* editor = tabs_.active_editor(); // borrowed
    if (editor == nullptr) {
        return std::nullopt;
    }
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(editor->buffer());
    GtkTextIter start;
    GtkTextIter end;
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end) == FALSE) {
        return std::nullopt;
    }
    if (gtk_text_iter_get_line(&start) != gtk_text_iter_get_line(&end)) {
        return std::nullopt;
    }
    if (gtk_text_iter_get_offset(&end) - gtk_text_iter_get_offset(&start) > kMaxSelectionChars) {
        return std::nullopt;
    }
    return gtkx::take_string(gtk_text_buffer_get_text(buffer, &start, &end, FALSE));
}

void MainWindow::focus_active_view() {
    gtk_window_present(window_);
    EditorPage* editor = tabs_.active_editor(); // borrowed
    if (editor != nullptr) {
        gtk_widget_grab_focus(editor->view());
    }
}

GtkTextView* MainWindow::active_text_view() const {
    EditorPage* editor = tabs_.active_editor(); // borrowed
    return editor != nullptr ? GTK_TEXT_VIEW(editor->view()) : nullptr;
}

void MainWindow::find() {
    open_find(selected_search_text());
}

void MainWindow::replace() {
    if (replace_.job_active() && replace_window_) {
        replace_window_->present(); // the Cancel button of the running job is there
        return;
    }
    open_replace(selected_search_text());
}

void MainWindow::find_next() {
    find_again(true);
}

void MainWindow::find_previous() {
    find_again(false);
}

void MainWindow::find_again(bool forward) {
    if (tabs_.active_editor() == nullptr || replace_.job_active()) {
        return;
    }
    sync_search_buffer();
    if (!search_.has_text()) {
        open_find(selected_search_text());
        return;
    }
    FindResult result = forward ? search_.find_next() : search_.find_previous();
    GtkTextView* view = active_text_view(); // borrowed
    if (result.found && view != nullptr) {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
        gtk_text_view_scroll_to_mark(view, gtk_text_buffer_get_insert(buffer), 0.1, FALSE, 0.0,
                                     0.0);
    }
    // The message for the main status bar: with the windows closed, nothing else shows it.
    std::string message = search_.status();
    const SearchSession* session = search_.session(); // borrowed
    if (result.found && !result.wrapped && session != nullptr && session->searching()) {
        message.clear(); // the scan has not ended, so there is no count to show yet
    } else if (!result.found && !search_.status_is_problem()) {
        message = match_count_message(0, 0); // "Not found"
    }
    status_.show_message(message);
}

void MainWindow::go_to_line() {
    if (tabs_.active_editor() == nullptr) {
        return;
    }
    if (!goto_) {
        GotoLineWindow::Host host;
        host.active_view = [this] { return active_text_view(); };
        host.closed = [this] { focus_active_view(); };
        host.message = [this](const std::string& message) { status_.show_message(message); };
        goto_ = std::make_unique<GotoLineWindow>(window_, std::move(host));
        goto_destroy_ = gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(goto_->window()), "destroy",
                                                        [this](GtkWidget*) { goto_.reset(); });
    }
    goto_->open();
}

void MainWindow::open_find(const std::optional<std::string>& selection) {
    if (tabs_.active_editor() == nullptr || replace_.job_active()) {
        return;
    }
    sync_search_buffer();
    if (replace_window_ && replace_window_->is_open()) {
        replace_window_->hide();
    }
    if (!find_) {
        FindWindow::Host host;
        host.active_view = [this] { return active_text_view(); };
        host.closed = [this] { focus_active_view(); };
        host.show_replace = [this] { open_replace(std::nullopt); };
        find_ = std::make_unique<FindWindow>(window_, search_, std::move(host));
        find_destroy_ = gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(find_->window()), "destroy",
                                                        [this](GtkWidget*) { find_.reset(); });
    }
    find_->open(selection);
}

void MainWindow::open_replace(const std::optional<std::string>& selection) {
    if (tabs_.active_editor() == nullptr || replace_.job_active()) {
        return;
    }
    sync_search_buffer();
    if (find_ && find_->is_open()) {
        find_->hide();
    }
    if (!replace_window_) {
        ReplaceWindow::Host host;
        host.active_view = [this] { return active_text_view(); };
        host.closed = [this] { focus_active_view(); };
        host.show_find = [this] { open_find(std::nullopt); };
        host.job_started = [this] { replace_job_started(); };
        host.job_ended = [this] { replace_job_ended(); };
        host.message = [this](const std::string& message) { status_.show_message(message); };
        replace_window_ =
            std::make_unique<ReplaceWindow>(window_, search_, replace_, std::move(host));
        replace_destroy_ =
            gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(replace_window_->window()), "destroy",
                                            [this](GtkWidget*) { replace_window_.reset(); });
    }
    replace_window_->open(selection);
}

// The Replace window started a job: the document is locked, so that nothing edits, saves or
// reloads it while the slices run, and the actions that must not run meanwhile are greyed out.
// With no document to lock, or one that loads or saves, the job is ended at once.
void MainWindow::replace_job_started() {
    std::optional<DocumentId> active = list_.active();
    EditorPage* editor = active ? tabs_.editor(*active) : nullptr; // borrowed
    if (editor == nullptr || editor->busy()) {
        replace_.cancel_job();
        return;
    }
    editor->set_locked(true);
    job_document_ = active;
    update_state();
}

// The job ended (a Cancel, its last slice, a tab change or a closed tab): the document is
// unlocked when it still exists. Only the actions are brought up to date: this can run inside
// refresh(), from the search's change of buffer, which must not be started again.
void MainWindow::replace_job_ended() {
    if (job_document_) {
        EditorPage* editor = tabs_.editor(*job_document_); // borrowed; null for a closed tab
        if (editor != nullptr) {
            editor->set_locked(false);
        }
        job_document_.reset();
    }
    update_state();
}

void MainWindow::end_replace_job() {
    if (replace_.job_active()) {
        replace_.cancel_job();
    }
}

} // namespace vermilion::ui
