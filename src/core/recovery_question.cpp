// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the texts of the start-up question about a recovery copy and judges its file's time.

#include "core/recovery_question.h"

#include <glib.h>

#include "core/open_messages.h"

namespace vermilion {

std::optional<std::string> recovery_path(const RecoveryHeader& header) {
    if (!header.uri.has_value()) {
        return std::nullopt;
    }
    g_autoptr(GError) error = nullptr;
    g_autofree char* path = g_filename_from_uri(header.uri->c_str(), nullptr, &error);
    if (path == nullptr) {
        return std::nullopt;
    }
    return std::string(path);
}

std::string recovery_name(const RecoveryHeader& header) {
    if (auto path = recovery_path(header); path.has_value()) {
        g_autofree char* base = g_path_get_basename(path->c_str());
        g_autofree char* display = g_filename_display_name(base);
        return std::string(display);
    }
    if (header.title.has_value()) {
        return header.title.value();
    }
    if (header.uri.has_value()) {
        return header.uri.value();
    }
    return "Untitled";
}

FileState file_state(const RecoveryHeader& header, std::optional<std::int64_t> current_mtime_us) {
    if (!header.uri.has_value()) {
        return FileState::Untitled;
    }
    if (!current_mtime_us.has_value()) {
        return FileState::Missing;
    }
    if (!header.file_mtime_us.has_value()) {
        return FileState::Unknown;
    }
    if (*current_mtime_us == *header.file_mtime_us) {
        return FileState::Unchanged;
    }
    return FileState::Changed;
}

std::string format_recovery_time(std::int64_t microseconds, bool local_time) {
    if (microseconds < 0) {
        return "an unknown time";
    }
    g_autoptr(GDateTime) utc = g_date_time_new_from_unix_utc(microseconds / 1000000);
    if (utc == nullptr) {
        return "an unknown time";
    }
    GDateTime* chosen = local_time ? g_date_time_to_local(utc) : g_date_time_ref(utc);
    if (chosen == nullptr) {
        return "an unknown time";
    }
    g_autoptr(GDateTime) dt = chosen;
    g_autofree char* text = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
    if (text == nullptr) {
        return "an unknown time";
    }
    return std::string(text);
}

RecoveryQuestion recovery_question(const RecoveryItem& item, std::size_t number, std::size_t total,
                                   std::optional<std::int64_t> current_mtime_us, bool local_time) {
    std::string place =
        total > 1 ? " (" + std::to_string(number) + " of " + std::to_string(total) + ")" : "";
    if (item.damaged) {
        RecoveryQuestion question;
        question.message = "A recovery copy is damaged." + place;
        question.detail = "The copy " + item.id + ".vrec cannot be used: " + item.problem +
                          "\nDecide Later keeps it; Discard deletes it.";
        return question;
    }
    const RecoveryHeader& header = item.header;
    RecoveryQuestion question;
    question.message = "Recover unsaved changes to " + quoted(recovery_name(header)) + "?" + place;
    question.detail =
        "The copy was made " + format_recovery_time(header.copied_us, local_time) + ".";
    FileState state = file_state(header, current_mtime_us);
    if (state == FileState::Untitled) {
        question.detail += "\nThe document was never saved to a file.";
        return question;
    }
    question.detail += "\nFile: ";
    if (auto path = recovery_path(header); path.has_value()) {
        g_autofree char* display = g_filename_display_name(path->c_str());
        question.detail += display;
    } else {
        question.detail += header.uri.value();
    }
    switch (state) {
    case FileState::Unchanged:
        question.detail += "\nThe file has not changed since the copy's text was based on it.";
        break;
    case FileState::Changed: {
        std::string now = format_recovery_time(*current_mtime_us, local_time);
        question.detail += "\nThe file was last changed " + now +
                           ", after the copy's text was based on it. Saving will ask before it "
                           "overwrites the file.";
        break;
    }
    case FileState::Missing:
        question.detail +=
            "\nThe file no longer exists or cannot be read. Saving will write it again.";
        break;
    case FileState::Unknown: {
        std::string now = format_recovery_time(*current_mtime_us, local_time);
        question.detail += "\nThe copy does not record which version of the file its text was "
                           "based on. The file was last changed " +
                           now + ".";
        break;
    }
    case FileState::Untitled:
        break;
    }
    return question;
}

} // namespace vermilion
