// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// File > Open: the file dialog, the check of the file's bytes, and the question before opening.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>

#include "core/text_scan.h"
#include "ui/file_probe.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

class FileOpener {
public:
    struct Callbacks {
        std::function<void(const std::string&)> message; // a line for the status bar
        // An error for a dialog: message and detail.
        std::function<void(const std::string&, const std::string&)> error;
        // Selects the file's tab if it is already open; true when it was.
        std::function<bool(const std::string&)> select_if_open;
        // Opens the checked file (path, title, scan) in a new tab.
        std::function<void(const std::string&, const std::string&, const ScanResult&)> open;
    };

    // parent is borrowed; it must outlive this object.
    FileOpener(GtkWindow* parent, Callbacks callbacks);
    ~FileOpener();

    FileOpener(const FileOpener&) = delete;
    FileOpener& operator=(const FileOpener&) = delete;

    // Shows the file dialog, then opens the chosen file.
    void choose();

    // Checks the file at path (absolute) and opens it, asking first when it looks binary or has a
    // very long line. Only one file is checked at a time; a new call replaces the one running.
    void open_path(const std::string& path);

private:
    static void on_chosen(GObject* source, GAsyncResult* result, gpointer ticket);
    void on_checked(const std::string& path, const std::string& title,
                    const FileProbe::Result& result);

    GtkWindow* parent_; // borrowed
    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_; // cancels an open file dialog
    std::unique_ptr<FileProbe> probe_;
    gtkx::AsyncGuard<FileOpener> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
