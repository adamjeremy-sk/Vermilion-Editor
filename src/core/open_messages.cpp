// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the texts shown while a file is opened: the question, progress lines and the result.

#include "core/open_messages.h"

#include "core/text_format.h"

namespace vermilion {

std::string quoted(const std::string& title) {
    return "\u201c" + title + "\u201d";
}

OpenQuestion open_question(const std::string& title, OpenWarning warning,
                           std::uint64_t longest_line) {
    OpenQuestion question;
    switch (warning) {
    case OpenWarning::Binary:
        question.message = quoted(title) + " looks like a binary file.";
        question.detail = "It contains NUL characters. If you open it anyway, it opens read-only.";
        break;
    case OpenWarning::LongLine:
        question.message =
            quoted(title) + " has a line of " + std::to_string(longest_line) + " characters.";
        question.detail = "Lines this long can make the editor slow or stop responding.";
        break;
    case OpenWarning::None:
        break;
    }
    return question;
}

std::string checking_message(const std::string& title, int percent) {
    return "Checking " + quoted(title) + "\u2026 " + std::to_string(percent) + "%";
}

std::string loading_message(const std::string& title, int percent) {
    return "Loading " + quoted(title) + "\u2026 " + std::to_string(percent) + "%";
}

std::string loaded_message(const DocumentRecord& record) {
    if (record.read_only) {
        return "Opened " + quoted(record.title) +
               " read-only: it contains bytes that cannot be shown as text.";
    }
    std::string message = "Opened " + quoted(record.title);
    if (record.format.encoding != TextEncoding::Utf8 || record.format.bom) {
        message += " as " + encoding_label(record.format);
    }
    if (record.mixed_line_endings) {
        message += ". It has mixed line endings; saving makes them all ";
        message += line_ending_label(record.format.line_ending);
    }
    message += ".";
    return message;
}

} // namespace vermilion
