// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs GtkSourceFileSaver with the document's format and sorts its result into an Outcome.

#include "ui/document_saver.h"

#include <memory>
#include <utility>

#include "core/log.h"

namespace vermilion::ui {

namespace {

GtkSourceNewlineType newline_type(LineEnding ending) {
    switch (ending) {
    case LineEnding::Lf:
        return GTK_SOURCE_NEWLINE_TYPE_LF;
    case LineEnding::CrLf:
        return GTK_SOURCE_NEWLINE_TYPE_CR_LF;
    case LineEnding::Cr:
        return GTK_SOURCE_NEWLINE_TYPE_CR;
    }
    return GTK_SOURCE_NEWLINE_TYPE_LF;
}

} // namespace

// Full references from _new (not floating); a second reference on file, the page keeps its own.
DocumentSaver::DocumentSaver(GtkSourceBuffer* buffer, GtkSourceFile* file, GFile* target,
                             const TextFormat& format, bool overwrite_changes, Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())),
      file_(gtkx::ObjectPtr<GtkSourceFile>::ref(file)),
      target_(gtkx::ObjectPtr<GFile>::ref(target)), started_(g_get_monotonic_time()) {
    GFile* location = gtk_source_file_get_location(file); // borrowed, may be null
    bool plain = location != nullptr && g_file_equal(location, target) != FALSE;
    if (plain) {
        saver_ =
            gtkx::ObjectPtr<GtkSourceFileSaver>::adopt(gtk_source_file_saver_new(buffer, file));
    } else {
        saver_ = gtkx::ObjectPtr<GtkSourceFileSaver>::adopt(
            gtk_source_file_saver_new_with_target(buffer, file, target));
    }
    const GtkSourceEncoding* encoding =
        gtk_source_encoding_get_from_charset(charset_name(format.encoding)); // borrowed, static
    if (encoding != nullptr) {
        gtk_source_file_saver_set_encoding(saver_.get(), encoding);
    }
    gtk_source_file_saver_set_newline_type(saver_.get(), newline_type(format.line_ending));
    GtkSourceFileSaverFlags flags = overwrite_changes
                                        ? GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME
                                        : GTK_SOURCE_FILE_SAVER_FLAGS_NONE;
    gtk_source_file_saver_set_flags(saver_.get(), flags);
    gtk_source_file_saver_save_async(saver_.get(), G_PRIORITY_DEFAULT, cancellable_.get(),
                                     &DocumentSaver::on_progress, guard_.ticket(),
                                     &gtkx::AsyncGuard<DocumentSaver>::free_ticket,
                                     &DocumentSaver::on_saved, guard_.ticket());
}

DocumentSaver::~DocumentSaver() {
    g_cancellable_cancel(cancellable_.get());
}

void DocumentSaver::on_progress(goffset current, goffset total, gpointer ticket) {
    DocumentSaver* self = gtkx::AsyncGuard<DocumentSaver>::peek(ticket);
    if (self == nullptr || total <= 0) {
        return;
    }
    int percent = static_cast<int>(current >= total ? 100 : current * 100 / total);
    if (percent != self->percent_) {
        self->percent_ = percent;
        self->callbacks_.progress(percent);
    }
}

void DocumentSaver::on_saved(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    gboolean ok = gtk_source_file_saver_save_finish(GTK_SOURCE_FILE_SAVER(source), result, &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    DocumentSaver* self = gtkx::AsyncGuard<DocumentSaver>::take(ticket);
    if (self == nullptr) {
        return;
    }
    Outcome outcome = Outcome::Saved;
    std::string message;
    if (ok) {
        gtk_source_file_set_location(self->file_.get(), self->target_.get());
        std::string ms = std::to_string((g_get_monotonic_time() - self->started_) / 1000);
        logging::debug("saved in " + ms + " ms");
    } else {
        bool changed = error && error->domain == GTK_SOURCE_FILE_SAVER_ERROR &&
                       error->code == GTK_SOURCE_FILE_SAVER_ERROR_EXTERNALLY_MODIFIED;
        outcome = changed ? Outcome::ChangedOnDisk : Outcome::Failed;
        message = error ? error->message : "Unknown error";
    }
    // The callback may destroy this object, so it runs last, from a local copy.
    std::function<void(Outcome, const std::string&)> finished =
        std::move(self->callbacks_.finished);
    finished(outcome, message);
}

} // namespace vermilion::ui
