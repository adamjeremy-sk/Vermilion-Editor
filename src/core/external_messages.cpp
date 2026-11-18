// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the texts shown when a file changes on disk.

#include "core/external_messages.h"

#include "core/open_messages.h"
#include "core/save_messages.h"

namespace vermilion {

std::string reloaded_message(const std::string& title) {
    return "Reloaded " + quoted(title) + " (changed on disk)";
}

std::string reloaded_encoding_message(const std::string& title, const std::string& label) {
    return reloaded_message(title) + "; the encoding is now " + label;
}

std::string deleted_message(const std::string& title) {
    return quoted(title) + " was deleted or moved on disk";
}

std::string banner_text(BannerKind kind, const std::string& title) {
    switch (kind) {
    case BannerKind::Changed:
        return quoted(title) + " changed on disk.";
    case BannerKind::ChangedLarge:
        return quoted(title) + " changed on disk. It is large, so it was not reloaded.";
    case BannerKind::Returned:
        return quoted(title) + " is back on disk after it was deleted or moved.";
    case BannerKind::None:
        break;
    }
    return std::string();
}

BannerButtons banner_buttons(BannerKind kind) {
    switch (kind) {
    case BannerKind::Changed:
    case BannerKind::Returned:
        return BannerButtons{"Reload", "Keep mine"};
    case BannerKind::ChangedLarge:
        return BannerButtons{"Reload", "Ignore"};
    case BannerKind::None:
        break;
    }
    return BannerButtons{};
}

std::string quit_changed_line() {
    return "This file also changed on disk.";
}

OpenQuestion close_question_for(const std::string& title, CloseCase kind) {
    OpenQuestion question = close_question(title);
    switch (kind) {
    case CloseCase::Edited:
        break;
    case CloseCase::EditedFileChanged:
        question.detail += "\n" + quit_changed_line();
        break;
    case CloseCase::FileGone:
        question.message = "Save " + quoted(title) + " before closing?";
        question.detail = "The file was deleted or moved on disk. If you don't save, its text "
                          "will be lost.";
        break;
    }
    return question;
}

ReloadQuestion reload_question(const std::string& title, ReloadAsk kind, std::uint64_t disk_size) {
    ReloadQuestion question;
    switch (kind) {
    case ReloadAsk::DiscardChanges:
        question.message = "Discard your changes and reload from disk?";
        question.detail = "Your unsaved changes to " + quoted(title) + " will be lost.";
        question.go = "Discard and Reload";
        break;
    case ReloadAsk::LargeFile:
        question.message = "Reload " + quoted(title) + " from disk?";
        question.detail = "It is large (" + size_in_megabytes(disk_size) +
                          "), so this may take a "
                          "moment.";
        question.go = "Reload";
        break;
    case ReloadAsk::Nothing:
        break;
    }
    return question;
}

std::string dont_ask_again_label() {
    return "Do not ask again";
}

std::string size_in_megabytes(std::uint64_t bytes) {
    const std::uint64_t tenths = (bytes * 10 + 524288) / 1048576;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " MB";
}

std::string reload_busy_message(const std::string& title) {
    return quoted(title) + " is busy. Try Reload from Disk again in a moment.";
}

std::string reload_missing_message(const std::string& title) {
    return quoted(title) + " is not on disk, so it was not reloaded.";
}

std::string reload_failed_message(const std::string& title) {
    return "Could not reload " + quoted(title) + ".";
}

std::string settings_save_failed_message(const std::string& error) {
    return "Could not save the settings: " + error;
}

std::string reloaded_by_hand_message(const std::string& title) {
    return "Reloaded " + quoted(title) + " from disk";
}

std::string reloaded_by_hand_encoding_message(const std::string& title, const std::string& label) {
    return reloaded_by_hand_message(title) + "; the encoding is now " + label;
}

} // namespace vermilion
