// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Runs GtkSourceFileLoader with a single candidate encoding and sorts its result into an Outcome.

#include "ui/document_loader.h"

#include <memory>
#include <utility>

namespace vermilion::ui {

// Full references from _new (not floating). The candidate list is copied by the loader, so a
// one-element list on the stack is enough.
DocumentLoader::DocumentLoader(GtkSourceBuffer* buffer, GtkSourceFile* file, TextEncoding encoding,
                               Callbacks callbacks)
    : callbacks_(std::move(callbacks)),
      cancellable_(gtkx::ObjectPtr<GCancellable>::adopt(g_cancellable_new())),
      loader_(
          gtkx::ObjectPtr<GtkSourceFileLoader>::adopt(gtk_source_file_loader_new(buffer, file))) {
    const GtkSourceEncoding* source_encoding =
        gtk_source_encoding_get_from_charset(charset_name(encoding)); // borrowed, static
    GSList candidates = {const_cast<GtkSourceEncoding*>(source_encoding), nullptr};
    if (source_encoding != nullptr) {
        gtk_source_file_loader_set_candidate_encodings(loader_.get(), &candidates);
    }
    gtk_source_file_loader_load_async(loader_.get(), G_PRIORITY_DEFAULT, cancellable_.get(),
                                      &DocumentLoader::on_progress, guard_.ticket(),
                                      &gtkx::AsyncGuard<DocumentLoader>::free_ticket,
                                      &DocumentLoader::on_loaded, guard_.ticket());
}

DocumentLoader::~DocumentLoader() {
    g_cancellable_cancel(cancellable_.get());
}

void DocumentLoader::on_progress(goffset current, goffset total, gpointer ticket) {
    DocumentLoader* self = gtkx::AsyncGuard<DocumentLoader>::peek(ticket);
    if (self == nullptr || total <= 0) {
        return;
    }
    int percent = static_cast<int>(current >= total ? 100 : current * 100 / total);
    if (percent != self->percent_) {
        self->percent_ = percent;
        self->callbacks_.progress(percent);
    }
}

void DocumentLoader::on_loaded(GObject* source, GAsyncResult* result, gpointer ticket) {
    GError* raw = nullptr;
    gboolean ok = gtk_source_file_loader_load_finish(GTK_SOURCE_FILE_LOADER(source), result, &raw);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw, &g_error_free);
    DocumentLoader* self = gtkx::AsyncGuard<DocumentLoader>::take(ticket);
    if (self == nullptr) {
        return;
    }
    Outcome outcome = Outcome::Loaded;
    std::string message;
    if (!ok) {
        bool fallback = error && error->domain == GTK_SOURCE_FILE_LOADER_ERROR &&
                        error->code == GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK;
        outcome = fallback ? Outcome::LoadedWithInvalidBytes : Outcome::Failed;
        message = error ? error->message : "Unknown error";
    }
    // The callback may destroy this object, so it runs last, from a local copy.
    std::function<void(Outcome, const std::string&)> finished =
        std::move(self->callbacks_.finished);
    finished(outcome, message);
}

} // namespace vermilion::ui
