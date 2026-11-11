// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs the open dialog, checks the chosen file with a FileProbe and asks before odd files open.

#include "ui/file_opener.h"

#include <utility>

#include "core/open_messages.h"
#include "ui/close_dialog.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {

// Full reference from _new (not floating).
FileOpener::FileOpener(GtkWindow* parent, Callbacks callbacks)
    : parent_(parent), callbacks_(std::move(callbacks)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())) {}

FileOpener::~FileOpener() {
    g_cancellable_cancel(cancellable_.get());
}

// Full reference from _new (not floating); open() keeps its own while the dialog is shown.
void FileOpener::choose() {
    auto dialog = gtkx::ObjectPtr<GtkFileDialog>::adopt(gtk_file_dialog_new());
    gtk_file_dialog_set_title(dialog.get(), "Open");
    gtk_file_dialog_set_modal(dialog.get(), TRUE);
    // The dialog starts in the home folder, not in "Recent". g_get_home_dir() is borrowed.
    auto home = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(g_get_home_dir()));
    gtk_file_dialog_set_initial_folder(dialog.get(), home.get());
    gtk_file_dialog_open(dialog.get(), parent_, cancellable_.get(), &FileOpener::on_chosen,
                         guard_.ticket());
}

// A dismissed dialog (Cancel, Escape) is not an error. A location without a local path (a
// remote location) cannot be opened yet.
void FileOpener::on_chosen(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    auto file = gtkx::ObjectPtr<GFile>::adopt(
        gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &raw));
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    FileOpener* self = gtkx::AsyncGuard<FileOpener>::take(ticket);
    if (self == nullptr || !file) {
        return;
    }
    std::string path = gtkx::take_string(g_file_get_path(file.get()));
    if (path.empty()) {
        std::string name = gtkx::take_string(g_file_get_parse_name(file.get()));
        self->callbacks_.error("Could not open " + quoted(name),
                               "Only files on this computer can be opened.");
        return;
    }
    self->open_path(path);
}

void FileOpener::open_path(const std::string& path) {
    if (callbacks_.select_if_open(path)) {
        return;
    }
    std::string title = gtkx::take_string(g_filename_display_basename(path.c_str()));
    probe_ = std::make_unique<FileProbe>(
        path, FileProbe::Callbacks{[this, title](int percent) {
                                       callbacks_.message(checking_message(title, percent));
                                   },
                                   [this, path, title](const FileProbe::Result& result) {
                                       on_checked(path, title, result);
                                   }});
}

// Runs from the probe's own callback; resetting probe_ destroys it, which is allowed there, so the
// arguments are copied first.
void FileOpener::on_checked(const std::string& path, const std::string& title,
                            const FileProbe::Result& result) {
    std::string file_path = path;
    std::string file_title = title;
    FileProbe::Result checked = result;
    probe_.reset();
    if (!checked.ok) {
        callbacks_.message("Could not open " + quoted(file_title) + ".");
        callbacks_.error("Could not open " + quoted(file_title), checked.error);
        return;
    }
    OpenWarning warning = open_warning(checked.scan, kDefaultLongLineLimit);
    if (warning == OpenWarning::None) {
        callbacks_.open(file_path, file_title, checked.scan);
        return;
    }
    OpenQuestion question = open_question(file_title, warning, checked.scan.longest_line);
    callbacks_.message(question.message);
    ask_discard(parent_, question.message, question.detail, "Open Anyway",
                [this, file_path, file_title, scan = checked.scan] {
                    callbacks_.open(file_path, file_title, scan);
                });
}

} // namespace vermilion::ui
