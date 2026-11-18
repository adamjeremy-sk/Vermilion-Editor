// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// One file monitor and a debounce timer: any event on the file means "look again".

#include "ui/file_watcher.h"

#include <utility>

namespace vermilion::ui {

FileWatcher::FileWatcher(std::unique_ptr<DebounceTimer> timer, std::function<void()> look,
                         unsigned delay_ms)
    : timer_(std::move(timer)), look_(std::move(look)), delay_ms_(delay_ms) {}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::watch(const std::string& path) {
    if (path.empty()) {
        stop();
        return;
    }
    if (static_cast<bool>(monitor_) && path == path_) {
        return;
    }
    stop();
    auto file = gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(path.c_str()));
    GError* raw = nullptr;
    GFileMonitor* monitor = g_file_monitor_file(file.get(), G_FILE_MONITOR_NONE, nullptr, &raw);
    if (raw != nullptr) {
        g_error_free(raw);
    }
    if (monitor == nullptr) {
        return;
    }
    monitor_ = gtkx::ObjectPtr<GFileMonitor>::adopt(monitor);
    path_ = path;
    changed_ = gtkx::connect<void(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent)>(
        monitor_.get(), "changed",
        [this](GFileMonitor*, GFile*, GFile*, GFileMonitorEvent) { on_event(); });
}

void FileWatcher::stop() {
    timer_->stop();
    changed_.disconnect();
    if (static_cast<bool>(monitor_)) {
        g_file_monitor_cancel(monitor_.get());
    }
    monitor_ = gtkx::ObjectPtr<GFileMonitor>();
    path_.clear();
}

void FileWatcher::on_event() {
    timer_->start(delay_ms_, [this] { look_(); });
}

} // namespace vermilion::ui
