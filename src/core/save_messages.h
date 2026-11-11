// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The texts shown while a document is saved, closed or reopened: questions and status lines.

#pragma once

#include <string>

#include "core/open_messages.h"
#include "core/text_format.h"

namespace vermilion {

// A question or an error for a dialog uses OpenQuestion (message and detail). T is the title.

// "Save changes to \u201cT\u201d before closing?" / "If you don't save, your changes will be
// lost."
OpenQuestion close_question(const std::string& title);

// "\u201cT\u201d has changed on disk since it was opened." / "Saving replaces the version on disk
// with this one."
OpenQuestion changed_on_disk_question(const std::string& title);

// "Reopen \u201cT\u201d as E?" / "Your unsaved changes will be lost." (E is the encoding label.)
OpenQuestion reopen_question(const std::string& title, const std::string& encoding);

// "\u201cT\u201d is open in another tab." / "Close that tab first, or choose another name."
OpenQuestion already_open_error(const std::string& title);

// "Could not save \u201cT\u201d" with the reason as the detail.
OpenQuestion save_error(const std::string& title, const std::string& reason);

// "Saving \u201cT\u201d\u2026 P%"
std::string saving_message(const std::string& title, int percent);

// "Saved \u201cT\u201d."
std::string saved_message(const std::string& title);

// "Wait until \u201cT\u201d has finished loading or saving."
std::string busy_message(const std::string& title);

// "The next save writes E with L line endings." (E the encoding label, L "LF", "CRLF" or "CR")
std::string next_save_message(const TextFormat& format);

} // namespace vermilion
