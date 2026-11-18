// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Watches one file for any change and says "look at it again" once the events have settled.

#pragma once

#include <gio/gio.h>

#include <functional>
#include <memory>
#include <string>

#include "ui/debounce_timer.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// One GFileMonitor on the file at a path, plus a debounce timer. The kind of event is never
// looked at: a write, a touch, a rename over the file, a delete and a move all mean the same
// thing, "look at the file again", and the caller decides from the file's state (see
// core/external_change.h). The monitor keeps working after the file is deleted and created again.
// Main thread only; nothing here touches a widget.
class FileWatcher {
public:
    // The pause after the last event before `look` runs: events come in bursts (a save writes,
    // renames and changes attributes), and one look is enough.
    static constexpr unsigned kDefaultDelayMs = 300;

    // `look` runs from the main loop, once, delay_ms after the last event of a burst. It must not
    // destroy the watcher. `timer` is a GlibTimer in the program and a fake in tests.
    FileWatcher(std::unique_ptr<DebounceTimer> timer, std::function<void()> look,
                unsigned delay_ms = kDefaultDelayMs);

    // Stops watching; a destroyed watcher never calls `look`.
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Watches the file at path (absolute, as the file system names it); the file need not exist.
    // A path that is already watched is kept as it is. Any other path replaces the watched one, and
    // a look that was waiting for the old path is dropped. An empty path is stop(). When the
    // monitor cannot be created (the system's limit of watches, say) nothing is watched and
    // watching() is false.
    void watch(const std::string& path);

    // Stops watching and drops a look that is waiting; does nothing when nothing is watched.
    void stop();

    bool watching() const { return static_cast<bool>(monitor_); }

    // The path being watched; empty when none.
    const std::string& path() const { return path_; }

private:
    void on_event();

    std::unique_ptr<DebounceTimer> timer_;
    std::function<void()> look_;
    unsigned delay_ms_;
    std::string path_;
    gtkx::ObjectPtr<GFileMonitor> monitor_;
    gtkx::SignalConnection changed_;
};

} // namespace vermilion::ui
