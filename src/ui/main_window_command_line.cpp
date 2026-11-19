// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The main window's part of the command line: the files named at launch and what was refused.

#include <string>
#include <vector>

#include "ui/main_window.h"

namespace vermilion::ui {

void MainWindow::open_command_line(const std::vector<std::string>& paths,
                                   const std::vector<std::string>& messages) {
    std::string text;
    for (const std::string& message : messages) {
        if (!text.empty()) {
            text += "; ";
        }
        text += message;
    }
    if (paths.empty()) {
        if (!text.empty()) {
            status_.show_message(text);
        }
        return;
    }
    // The files are checked one after the other; the opening and the checking show their own
    // lines, so what was refused waits until the queue is empty.
    if (!text.empty()) {
        if (!pending_messages_.empty()) {
            pending_messages_ += "; ";
        }
        pending_messages_ += text;
    }
    close_blank_on_open_ = true;
    opener_.open_paths(paths);
}

// The first file that opens replaces the blank document of the start, if it is still untouched.
void MainWindow::before_file_opens() {
    if (close_blank_on_open_) {
        close_blank_on_open_ = false;
        close_blank_document();
    }
}

void MainWindow::on_open_queue_idle() {
    close_blank_on_open_ = false;
    if (!pending_messages_.empty()) {
        status_.show_message(pending_messages_);
        pending_messages_.clear();
    }
}

} // namespace vermilion::ui
