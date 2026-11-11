// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Loads one file into a GtkSourceBuffer with exactly one encoding, reporting progress and the end.

#pragma once

#include <gtksourceview/gtksource.h>

#include <functional>
#include <string>

#include "core/text_format.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

class DocumentLoader {
public:
    enum class Outcome {
        Loaded,
        LoadedWithInvalidBytes, // some bytes are shown as escapes; the document must not be saved
        Failed,
    };

    struct Callbacks {
        std::function<void(int)> progress; // percent loaded, only when it changes
        // Runs once; never after this object is gone. It may destroy this object.
        std::function<void(Outcome, const std::string&)> finished; // the error text when Failed
    };

    // Starts loading file (its location is set) into buffer. buffer and file must outlive this
    // object (borrowed). Destroying the object cancels the load; no callback runs afterwards.
    DocumentLoader(GtkSourceBuffer* buffer, GtkSourceFile* file, TextEncoding encoding,
                   Callbacks callbacks);
    ~DocumentLoader();

    DocumentLoader(const DocumentLoader&) = delete;
    DocumentLoader& operator=(const DocumentLoader&) = delete;

private:
    static void on_progress(goffset current, goffset total, gpointer ticket);
    static void on_loaded(GObject* source, GAsyncResult* result, gpointer ticket);

    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_;
    gtkx::ObjectPtr<GtkSourceFileLoader> loader_;
    int percent_ = -1;
    gtkx::AsyncGuard<DocumentLoader> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
