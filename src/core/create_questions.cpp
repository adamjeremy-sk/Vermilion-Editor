// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The questions about missing files, one after the other, and what became of each.

#include "core/create_questions.h"

#include <utility>

namespace vermilion {

CreateQuestions::CreateQuestions(std::vector<MissingFile> missing)
    : missing_(std::move(missing)), results_(missing_.size(), false), finished_(missing_.empty()) {}

QuestionAction CreateQuestions::action() const {
    if (finished_) {
        return QuestionAction::Finished;
    }
    return creating_ ? QuestionAction::Create : QuestionAction::Prompt;
}

const MissingFile* CreateQuestions::current() const {
    return finished_ ? nullptr : &missing_[index_];
}

std::string CreateQuestions::prompt() const {
    return finished_ ? std::string() : missing_prompt(missing_[index_].name);
}

void CreateQuestions::answer(const std::optional<std::string_view>& line) {
    if (finished_ || creating_) {
        return;
    }
    if (!line.has_value()) {
        finished_ = true;
        return;
    }
    switch (parse_answer(*line)) {
    case Answer::Yes:
        creating_ = true;
        break;
    case Answer::No:
        advance();
        break;
    case Answer::Ask:
        break;
    }
}

void CreateQuestions::created(bool ok) {
    if (finished_ || !creating_) {
        return;
    }
    results_[index_] = ok;
    advance();
}

void CreateQuestions::advance() {
    creating_ = false;
    ++index_;
    if (index_ >= missing_.size()) {
        finished_ = true;
    }
}

} // namespace vermilion
