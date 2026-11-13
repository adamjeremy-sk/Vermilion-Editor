// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// What the start-up question offers and says besides the question: buttons, text check, messages.

#include "core/recovery_answers.h"

#include <cstring>

#include <glib.h>

#include "core/open_messages.h"

namespace vermilion {

const char* recovery_answer_label(RecoveryAnswer answer) {
    switch (answer) {
    case RecoveryAnswer::Recover:
        return "Recover";
    case RecoveryAnswer::Discard:
        return "Discard";
    case RecoveryAnswer::RecoverAll:
        return "Recover All";
    case RecoveryAnswer::DecideLater:
        return "Decide Later";
    }
    return "";
}

std::vector<RecoveryAnswer> recovery_answers(const RecoveryItem& item, bool can_recover_all) {
    std::vector<RecoveryAnswer> answers;
    if (!item.damaged) {
        answers.push_back(RecoveryAnswer::Recover);
    }
    answers.push_back(RecoveryAnswer::Discard);
    if (!item.damaged && can_recover_all) {
        answers.push_back(RecoveryAnswer::RecoverAll);
    }
    answers.push_back(RecoveryAnswer::DecideLater);
    return answers;
}

std::optional<std::string> recovery_text_problem(std::string_view text) {
    if (!text.empty() && std::memchr(text.data(), '\0', text.size()) != nullptr) {
        return "the text contains a NUL character";
    }
    if (!g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr)) {
        return "the text is not valid UTF-8";
    }
    return std::nullopt;
}

RecoveryQuestion recovery_discard_question(const RecoveryItem& item) {
    RecoveryQuestion question;
    question.message = "Delete this copy?";
    if (item.damaged) {
        question.detail = "The damaged recovery copy " + item.id +
                          ".vrec will be deleted. This cannot be undone.";
    } else {
        question.detail = "The recovery copy of " + quoted(recovery_name(item.header)) +
                          " will be deleted. This cannot be undone.";
    }
    return question;
}

std::string recovered_untitled_message(const RecoveryHeader& header) {
    return "Recovered the text of " + quoted(recovery_name(header)) +
           " as an untitled document, because that file is already open.";
}

std::string recovery_kept_message(std::size_t kept) {
    if (kept == 0) {
        return "";
    }
    if (kept == 1) {
        return "1 recovery copy was kept. It is offered again at the next start.";
    }
    return std::to_string(kept) +
           " recovery copies were kept. They are offered again at the next start.";
}

} // namespace vermilion
