// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the texts shown when a document is saved, closed or reopened: questions and status lines.

#include "core/save_messages.h"

#include "core/text_format.h"

namespace vermilion {

OpenQuestion close_question(const std::string& title) {
    OpenQuestion question;
    question.message = "Save changes to " + quoted(title) + " before closing?";
    question.detail = "If you don't save, your changes will be lost.";
    return question;
}

OpenQuestion changed_on_disk_question(const std::string& title) {
    OpenQuestion question;
    question.message = quoted(title) + " has changed on disk since it was opened.";
    question.detail = "Saving replaces the version on disk with this one.";
    return question;
}

OpenQuestion reopen_question(const std::string& title, const std::string& encoding) {
    OpenQuestion question;
    question.message = "Reopen " + quoted(title) + " as " + encoding + "?";
    question.detail = "Your unsaved changes will be lost.";
    return question;
}

OpenQuestion already_open_error(const std::string& title) {
    OpenQuestion error;
    error.message = quoted(title) + " is open in another tab.";
    error.detail = "Close that tab first, or choose another name.";
    return error;
}

OpenQuestion save_error(const std::string& title, const std::string& reason) {
    OpenQuestion error;
    error.message = "Could not save " + quoted(title);
    error.detail = reason;
    return error;
}

std::string saving_message(const std::string& title, int percent) {
    return "Saving " + quoted(title) + "\u2026 " + std::to_string(percent) + "%";
}

std::string saved_message(const std::string& title) {
    return "Saved " + quoted(title) + ".";
}

std::string busy_message(const std::string& title) {
    return "Wait until " + quoted(title) + " has finished loading or saving.";
}

std::string next_save_message(const TextFormat& format) {
    return "The next save writes " + encoding_label(format) + " with " +
           line_ending_label(format.line_ending) + " line endings.";
}

} // namespace vermilion
