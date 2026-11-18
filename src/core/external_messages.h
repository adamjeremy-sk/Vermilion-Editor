// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The texts shown when a file changes on disk or is reloaded by hand: status lines, banner, quit
// line, close question, reload question.

#pragma once

#include <cstdint>
#include <string>

#include "core/external_change.h"
#include "core/open_messages.h"

namespace vermilion {

// The status line after a silent reload: "Reloaded \u201cT\u201d (changed on disk)".
std::string reloaded_message(const std::string& title);

// The status line after a silent reload that detected another encoding than the document had:
// "Reloaded \u201cT\u201d (changed on disk); the encoding is now <label>". label is an encoding
// label as encoding_label() gives it ("UTF-8", "Windows-1252", ...).
std::string reloaded_encoding_message(const std::string& title, const std::string& label);

// The status line when the file is found missing: "\u201cT\u201d was deleted or moved on disk".
std::string deleted_message(const std::string& title);

// The text of the banner, by kind:
// Changed:      "\u201cT\u201d changed on disk."
// ChangedLarge: "\u201cT\u201d changed on disk. It is large, so it was not reloaded."
// Returned:     "\u201cT\u201d is back on disk after it was deleted or moved."
// None:         empty.
std::string banner_text(BannerKind kind, const std::string& title);

// The two button labels of the banner.
struct BannerButtons {
    std::string reload;    // always "Reload"
    std::string secondary; // "Keep mine" for Changed and Returned, "Ignore" for ChangedLarge
};

// Both labels empty for None.
BannerButtons banner_buttons(BannerKind kind);

// The extra line in the quit question of an edited tab whose file also changed on disk:
// "This file also changed on disk."
std::string quit_changed_line();

// The question before a document closes, for the three cases of close_case():
// Edited:            close_question(T) unchanged (save_messages.h).
// EditedFileChanged: the message of Edited; the detail of Edited, a line feed and
//                    quit_changed_line().
// FileGone:          "Save \u201cT\u201d before closing?" /
//                    "The file was deleted or moved on disk. If you don't save, its text will be
//                    lost."
OpenQuestion close_question_for(const std::string& title, CloseCase kind);

// The question File > Reload from Disk asks, for the two kinds of reload_ask(). `go` is the label
// of the button that reloads (the other button is always Cancel). `disk_size` is the size of the
// file, used by LargeFile.
// DiscardChanges: "Discard your changes and reload from disk?" /
//                 "Your unsaved changes to \u201cT\u201d will be lost." / "Discard and Reload"
// LargeFile:      "Reload \u201cT\u201d from disk?" /
//                 "It is large (S), so this may take a moment." / "Reload"
//                 with S as size_in_megabytes(disk_size)
// Nothing:        all three empty.
struct ReloadQuestion {
    std::string message;
    std::string detail;
    std::string go;
};
ReloadQuestion reload_question(const std::string& title, ReloadAsk kind, std::uint64_t disk_size);

// The label of the check box of the LargeFile question: "Do not ask again".
std::string dont_ask_again_label();

// A size in megabytes (1 MB here is 1048576 bytes) with one decimal, rounded to the nearest tenth:
// 0 gives "0.0 MB", 1572864 gives "1.5 MB", 10485761 gives "10.0 MB".
std::string size_in_megabytes(std::uint64_t bytes);

// Status lines of File > Reload from Disk:
// busy:    "\u201cT\u201d is busy. Try Reload from Disk again in a moment."
// missing: "\u201cT\u201d is not on disk, so it was not reloaded."
// failed:  "Could not reload \u201cT\u201d."
std::string reload_busy_message(const std::string& title);
std::string reload_missing_message(const std::string& title);
std::string reload_failed_message(const std::string& title);

// The status line when the settings file cannot be written: "Could not save the settings: <error>".
std::string settings_save_failed_message(const std::string& error);

// The status line when File > Reload from Disk has reloaded a document: "Reloaded \u201cT\u201d
// from disk" (the file may not have changed, so it does not say "changed on disk").
std::string reloaded_by_hand_message(const std::string& title);

// The same when the reload detected another encoding than the document had: the message above, a
// semicolon and "the encoding is now <label>" (label as encoding_label() gives it).
std::string reloaded_by_hand_encoding_message(const std::string& title, const std::string& label);

} // namespace vermilion
