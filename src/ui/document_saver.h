// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Saves one GtkSourceBuffer with a chosen format, reporting progress and the end of the save.

#pragma once

#include <gtksourceview/gtksource.h>

#include <functional>
#include <string>

#include "core/text_format.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

class DocumentSaver {
public:
    enum class Outcome {
        Saved,
        ChangedOnDisk, // the file changed on disk since it was loaded or last saved; nothing
                       // written
        Failed,
    };

    struct Callbacks {
        std::function<void(int)> progress; // percent saved, only when it changes
        // Runs once; never after this object is gone. It may destroy this object.
        std::function<void(Outcome, const std::string&)> finished; // the error text when Failed
    };

    // Starts saving buffer to target with format (encoding, line ending; a byte-order mark is the
    // U+FEFF at the start of the text). When target is file's location this is a plain save;
    // otherwise it is a Save As, and after a successful save file's location is target. With
    // overwrite_changes, a file that changed on disk is overwritten instead of reported.
    // buffer and file are borrowed and must outlive this object; target is borrowed for the call.
    // Destroying the object cancels the save (GLib then leaves the file on disk as it was).
    DocumentSaver(GtkSourceBuffer* buffer, GtkSourceFile* file, GFile* target,
                  const TextFormat& format, bool overwrite_changes, Callbacks callbacks);
    ~DocumentSaver();

    DocumentSaver(const DocumentSaver&) = delete;
    DocumentSaver& operator=(const DocumentSaver&) = delete;

private:
    static void on_progress(goffset current, goffset total, gpointer ticket);
    static void on_saved(GObject* source, GAsyncResult* result, gpointer ticket);

    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_;
    gtkx::ObjectPtr<GtkSourceFile> file_; // the page's source file (a second reference)
    gtkx::ObjectPtr<GFile> target_;       // where the text goes
    gtkx::ObjectPtr<GtkSourceFileSaver> saver_;
    gint64 started_ = 0; // g_get_monotonic_time() at the start, for the debug log
    int percent_ = -1;
    gtkx::AsyncGuard<DocumentSaver> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
