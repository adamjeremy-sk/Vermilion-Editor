// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs saves and the questions around them; their endings are handled from the main loop.

#include "ui/save_controller.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/save_messages.h"
#include "ui/close_dialog.h"
#include "ui/editor_page.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

namespace {

constexpr int kSave = 0;
constexpr int kCloseWithoutSaving = 1;

void report(const std::function<void(bool)>& done, bool saved) {
    if (done) {
        done(saved);
    }
}

} // namespace

// Full reference from _new (not floating).
SaveController::SaveController(GtkWindow* parent, DocumentList& list, DocumentTabs& tabs,
                               Callbacks callbacks)
    : parent_(parent), list_(list), tabs_(tabs), callbacks_(std::move(callbacks)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())) {}

SaveController::~SaveController() {
    g_cancellable_cancel(cancellable_.get());
}

void SaveController::save(DocumentId id, std::function<void(bool)> done) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr || record->read_only) {
        report(done, false);
        return;
    }
    if (record->path.empty()) {
        save_as(id, std::move(done));
        return;
    }
    start(id, record->path, false, std::move(done));
}

// Full references from _new (not floating); save() keeps its own while the dialog is shown. Only
// one Save As dialog is open at a time.
void SaveController::save_as(DocumentId id, std::function<void(bool)> done) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr || record->read_only || choosing_ != 0) {
        report(done, false);
        return;
    }
    auto dialog = gtkx::ObjectPtr<GtkFileDialog>::adopt(gtk_file_dialog_new());
    gtk_file_dialog_set_title(dialog.get(), "Save As");
    gtk_file_dialog_set_modal(dialog.get(), TRUE);
    if (record->path.empty()) {
        // An untitled document is offered in the home folder. g_get_home_dir() is borrowed.
        auto home = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(g_get_home_dir()));
        gtk_file_dialog_set_initial_folder(dialog.get(), home.get());
        gtk_file_dialog_set_initial_name(dialog.get(), record->title.c_str());
    } else {
        auto file = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(record->path.c_str()));
        gtk_file_dialog_set_initial_file(dialog.get(), file.get());
    }
    choosing_ = id;
    choosing_done_ = std::move(done);
    gtk_file_dialog_save(dialog.get(), parent_, cancellable_.get(), &SaveController::on_chosen,
                         guard_.ticket());
}

// A dismissed dialog, or a location without a local path, cancels the save (and so a close or
// quit that waits for it).
void SaveController::on_chosen(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    auto file = gtkx::ObjectPtr<GFile>::adopt(
        gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &raw));
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    SaveController* self = gtkx::AsyncGuard<SaveController>::take(ticket);
    if (self == nullptr) {
        return;
    }
    DocumentId id = self->choosing_;
    std::function<void(bool)> done = std::move(self->choosing_done_);
    self->choosing_ = 0;
    self->choosing_done_ = nullptr;
    std::string path = file ? gtkx::take_string(g_file_get_path(file.get())) : std::string();
    if (path.empty()) {
        report(done, false);
        return;
    }
    const DocumentRecord* other = self->list_.find_by_path(path);
    if (other != nullptr && other->id != id) {
        OpenQuestion problem = already_open_error(other->title);
        self->callbacks_.error(problem.message, problem.detail);
        report(done, false);
        return;
    }
    self->start(id, path, false, std::move(done));
}

void SaveController::request_close(DocumentId id) {
    const DocumentRecord* record = list_.find(id);
    EditorPage* editor = tabs_.editor(id);
    if (record == nullptr || editor == nullptr) {
        return;
    }
    if (editor->saving()) {
        callbacks_.message(busy_message(record->title));
        return;
    }
    if (!record->modified) {
        tabs_.close(id);
        return;
    }
    OpenQuestion question = close_question(record->title);
    ask(parent_, question.message, question.detail, {"Save", "Close Without Saving", "Cancel"},
        [this, id](int button) {
            if (button == kSave) {
                save(id, [this, id](bool saved) {
                    if (saved) {
                        tabs_.close(id);
                    }
                });
            } else if (button == kCloseWithoutSaving) {
                tabs_.close(id);
            }
        });
}

void SaveController::ask_before_quit(std::function<void()> quit) {
    if (quitting_) {
        return;
    }
    quitting_ = true;
    quit_skipped_.clear();
    quit_ = std::move(quit);
    next_quit_question();
}

// The quit itself is deferred: it destroys the window, and with it this object.
void SaveController::next_quit_question() {
    std::vector<DocumentId> ids = list_.modified_ids();
    auto next = std::find_if(ids.begin(), ids.end(), [this](DocumentId id) {
        return std::find(quit_skipped_.begin(), quit_skipped_.end(), id) == quit_skipped_.end();
    });
    if (next == ids.end()) {
        quitting_ = false;
        defer(std::move(quit_));
        quit_ = nullptr;
        return;
    }
    DocumentId id = *next;
    tabs_.select(id);
    OpenQuestion question = close_question(list_.find(id)->title);
    ask(parent_, question.message, question.detail, {"Save", "Close Without Saving", "Cancel"},
        [this, id](int button) {
            if (button == kSave) {
                save(id, [this](bool saved) {
                    if (saved) {
                        next_quit_question();
                    } else {
                        quitting_ = false;
                    }
                });
            } else if (button == kCloseWithoutSaving) {
                quit_skipped_.push_back(id);
                next_quit_question();
            } else {
                quitting_ = false;
            }
        });
}

// Runs inside the saver's callback chain (EditorPage, DocumentTabs): the ending is handled from
// the main loop, where closing the tab or the window is safe.
void SaveController::on_saved(DocumentId id, DocumentSaver::Outcome outcome,
                              const std::string& error) {
    defer([this, id, outcome, error] { finish(id, outcome, error); });
}

void SaveController::start(DocumentId id, const std::string& path, bool overwrite_changes,
                           std::function<void(bool)> done) {
    const DocumentRecord* record = list_.find(id);
    EditorPage* editor = tabs_.editor(id);
    if (record == nullptr || editor == nullptr) {
        report(done, false);
        return;
    }
    if (editor->busy()) {
        callbacks_.message(busy_message(record->title));
        report(done, false);
        return;
    }
    pending_[id] = Pending{path, record->format, std::move(done)};
    editor->save(path, record->format, overwrite_changes);
    callbacks_.changed();
}

void SaveController::finish(DocumentId id, DocumentSaver::Outcome outcome,
                            const std::string& error) {
    auto it = pending_.find(id);
    if (it == pending_.end()) {
        return;
    }
    Pending pending = std::move(it->second);
    pending_.erase(it);
    const DocumentRecord* record = list_.find(id);
    callbacks_.changed();
    if (record == nullptr) {
        report(pending.done, false);
        return;
    }
    if (outcome == DocumentSaver::Outcome::ChangedOnDisk) {
        OpenQuestion question = changed_on_disk_question(record->title);
        ask(parent_, question.message, question.detail, {"Save Anyway", "Cancel"},
            [this, id, pending](int button) {
                if (button == 0) {
                    start(id, pending.path, true, pending.done);
                } else {
                    report(pending.done, false);
                }
            });
        return;
    }
    if (outcome == DocumentSaver::Outcome::Failed) {
        OpenQuestion problem = save_error(record->title, error);
        callbacks_.message(problem.message + ".");
        callbacks_.error(problem.message, problem.detail);
        report(pending.done, false);
        return;
    }
    if (pending.path != record->path) {
        list_.set_path(id, pending.path,
                       gtkx::take_string(g_filename_display_basename(pending.path.c_str())));
        tabs_.editor(id)->guess_language(pending.path);
    }
    list_.set_format(id, pending.format, false);
    list_.set_modified(id, false);
    tabs_.refresh_tab(id);
    callbacks_.message(saved_message(list_.find(id)->title));
    callbacks_.changed();
    report(pending.done, true);
}

// One deferred task per main-loop pass; the next pass is booked before the task runs, because a
// task may destroy this object (the guard then turns the booked pass into nothing).
void SaveController::defer(std::function<void()> work) {
    deferred_.push_back(std::move(work));
    if (!idle_pending_) {
        idle_pending_ = true;
        g_idle_add_once(&SaveController::run_deferred, guard_.ticket());
    }
}

void SaveController::run_deferred(gpointer ticket) {
    SaveController* self = gtkx::AsyncGuard<SaveController>::take(ticket);
    if (self == nullptr) {
        return;
    }
    self->idle_pending_ = false;
    if (self->deferred_.empty()) {
        return;
    }
    std::function<void()> work = std::move(self->deferred_.front());
    self->deferred_.erase(self->deferred_.begin());
    if (!self->deferred_.empty()) {
        self->idle_pending_ = true;
        g_idle_add_once(&SaveController::run_deferred, self->guard_.ticket());
    }
    work();
}

} // namespace vermilion::ui
