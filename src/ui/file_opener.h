// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// File > Open: the file dialog, the check of the file's bytes, and the question before opening.

#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/open_queue.h"
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
        // The queue is empty again: every file asked for is opened, refused or cancelled.
        std::function<void()> idle;
    };

    // parent is borrowed; it must outlive this object.
    FileOpener(GtkWindow* parent, Callbacks callbacks);
    ~FileOpener();

    FileOpener(const FileOpener&) = delete;
    FileOpener& operator=(const FileOpener&) = delete;

    // Shows the file dialog, then opens the chosen file.
    void choose();

    // Checks the files at paths (absolute), one after the other in this order, and opens them,
    // asking first when one looks binary or has a very long line. Files asked for while one is
    // being checked wait their turn.
    void open_paths(const std::vector<std::string>& paths);

    // The same for one file.
    void open_path(const std::string& path);

    // The longest line, in characters, that opens without a question (open_warning in
    // core/text_scan.h). Files that are checked after the call use it; until the first call it is
    // kDefaultLongLineLimit. The settings give it (ui/appearance_host.h).
    void set_long_line_limit(std::uint64_t limit) { long_line_limit_ = limit; }

private:
    static void on_chosen(GObject* source, GAsyncResult* result, gpointer ticket);
    void on_checked(const std::string& path, const std::string& title,
                    const FileProbe::Result& result);
    // Starts the next waiting file, until one needs time (a check) or none is left.
    void pump();
    // Starts the check of one file; false when nothing is left to do for it (already open).
    bool start(const std::string& path);
    // The file that was started is done; goes on with the next.
    void file_done();

    GtkWindow* parent_; // borrowed
    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_; // cancels an open file dialog
    OpenQueue queue_;
    std::unique_ptr<FileProbe> probe_;
    std::uint64_t long_line_limit_ = kDefaultLongLineLimit;
    gtkx::AsyncGuard<FileOpener> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
