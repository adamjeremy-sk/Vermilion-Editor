// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Reads a file once, block by block on the main loop, through a TextScanner before it is opened.

#pragma once

#include <gio/gio.h>

#include <cstdint>
#include <functional>
#include <string>

#include "core/text_scan.h"
#include "ui/gtkx/async_guard.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

class FileProbe {
public:
    struct Result {
        bool ok = false;
        ScanResult scan;   // valid when ok
        std::string error; // the reason, when not ok
    };

    struct Callbacks {
        std::function<void(int)> progress;           // percent read, only when it changes
        std::function<void(const Result&)> finished; // once; never after this object is gone
    };

    // Starts reading the file at path (absolute). The object may be destroyed at any time; that
    // cancels the reading and no callback runs afterwards.
    FileProbe(const std::string& path, Callbacks callbacks);
    ~FileProbe();

    FileProbe(const FileProbe&) = delete;
    FileProbe& operator=(const FileProbe&) = delete;

private:
    static void on_opened(GObject* source, GAsyncResult* result, gpointer ticket);
    static void on_info(GObject* source, GAsyncResult* result, gpointer ticket);
    static void on_read(GObject* source, GAsyncResult* result, gpointer ticket);
    void read_next();
    void finish(Result result);

    Callbacks callbacks_;
    gtkx::ObjectPtr<GCancellable> cancellable_;
    gtkx::ObjectPtr<GFileInputStream> stream_;
    TextScanner scanner_;
    std::uint64_t total_ = 0; // file size in bytes, 0 when unknown
    std::uint64_t read_ = 0;
    int percent_ = -1;
    gtkx::AsyncGuard<FileProbe> guard_{this}; // last member: it goes first
};

} // namespace vermilion::ui
