// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The start-up question about recovery copies: the walk through them and what each answer does.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/document_list.h"
#include "core/recovery_store.h"
#include "core/text_format.h"

namespace vermilion::ui {

// Asks about each recovery copy the previous run left (ARCHITECTURE.md 7.11), one at a time, and
// carries out the answer. It has no widget and includes no GTK header: the window is reached only
// through Host, so the whole walk runs in tests with a fake window, the real store and a real main
// loop. Everything runs on the main loop; a copy is read on a worker thread (ui/recovery_io.h).
// Every call to Host comes from the main loop and none comes after the prompt is destroyed: a
// question, a read or a file time that is still on its way then ends without effect.
class RecoveryPrompt {
public:
    // A document to build from a copy: the fields of DocumentTabs::Recovered. The text has passed
    // recovery_text_problem().
    struct Document {
        std::optional<std::string> path; // the local file the copy belongs to; none when untitled
        TextFormat format;
        bool mixed_line_endings = false;
        std::optional<std::int64_t> file_mtime_us; // the file time the copy's text was based on
        std::string text;
    };

    // What the window made of a Document: its id, and whether it took the path (false when the
    // document is untitled, because the file is already open in another tab).
    struct Added {
        DocumentId id = 0;
        bool has_path = false;
    };

    struct Host {
        // Shows a modal question and later calls on_answer once with the index of the chosen
        // button. The last button is Cancel: the default, the Escape answer and the answer when
        // the dialog is dismissed. May call on_answer before it returns; never calls it once the
        // window is gone (ui::ask of close_dialog.h, with the window bound).
        std::function<void(const std::string& message, const std::string& detail,
                           const std::vector<std::string>& buttons,
                           std::function<void(int)> on_answer)>
            ask;
        // Closes the blank document the program started with, if it is still untouched. Called
        // once, immediately before the first document is added.
        std::function<void()> close_blank;
        // Adds the document in a new tab, makes it active and returns its id.
        std::function<Added(Document)> add_document;
        // The window's RecoveryController::adopt: the document now owns the copy with this id.
        // False when the copy was not taken over, so it stays on disk.
        std::function<bool(DocumentId, const std::string& recovery_id,
                           std::optional<std::int64_t> file_mtime_us)>
            adopt;
        // A line for the status bar.
        std::function<void(const std::string&)> message;
    };

    RecoveryPrompt(RecoveryStore store, Host host);
    ~RecoveryPrompt();

    RecoveryPrompt(const RecoveryPrompt&) = delete;
    RecoveryPrompt& operator=(const RecoveryPrompt&) = delete;

    // Lists the copies and begins the walk. Does nothing when called a second time. With no copy
    // the walk ends at once. Use it once, after the window is shown.
    void start();

    // True once the walk has ended: every copy was recovered, deleted or kept, and the closing
    // message (if any) was sent. False before start().
    bool finished() const;

private:
    struct Core;
    std::shared_ptr<Core> core_;
};

} // namespace vermilion::ui
