// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The command line of a launch: its files, the questions on the terminal, the files created.

#pragma once

#include <gio/gio.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vermilion::ui {

// Handles the "command-line" signal for launches that name files (a launch without arguments is
// the caller's business). Each launch is sorted with make_open_request(); folders and other
// entries that cannot be opened are refused on the launching terminal; a name that does not exist
// is asked about on that terminal (Y/n) and the file is created on a yes. The questions are read
// asynchronously, so the main loop (and a window that is already open) keeps running while one
// waits. Without a terminal nothing is asked or created.
class CommandLineHandler {
public:
    struct Result {
        std::vector<std::string> paths;    // the files to open, in argument order
        std::vector<std::string> messages; // one line each for the status bar, in the order told
    };
    using Done = std::function<void(const Result&)>;

    // `app` is borrowed and must outlive this object. `done` runs on the main loop once per
    // launch, when every question of that launch is answered (at once when there are none).
    CommandLineHandler(GApplication* app, Done done);
    ~CommandLineHandler();

    CommandLineHandler(const CommandLineHandler&) = delete;
    CommandLineHandler& operator=(const CommandLineHandler&) = delete;

    // Takes up one launch with at least one argument after the program name. `cmdline` is
    // borrowed: the handler takes its own reference and drops it when the launch is done, which
    // lets the launching process end. The application is held until then.
    void handle(GApplicationCommandLine* cmdline);

private:
    class Launch;

    GApplication* app_; // borrowed
    Done done_;
    std::vector<std::unique_ptr<Launch>> launches_; // the finished ones go at the next launch
};

} // namespace vermilion::ui
