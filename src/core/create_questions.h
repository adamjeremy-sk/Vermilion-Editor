// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The questions about missing files, one after the other, and what became of each.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/open_arguments.h"

namespace vermilion {

// What the caller does now.
enum class QuestionAction {
    Prompt,   // show prompt(), read one line, then call answer()
    Create,   // create the file of current(), then call created()
    Finished, // no question is left
};

// A state machine with no I/O, for the terminal questions of the command line. The caller drives
// it with `switch (questions.action())` until it says Finished.
class CreateQuestions {
public:
    explicit CreateQuestions(std::vector<MissingFile> missing);

    // Prompt while a file is being asked about, Create after a yes, Finished when no file is left
    // (also at once for an empty list).
    QuestionAction action() const;

    // The file being asked about; null when Finished.
    const MissingFile* current() const;

    // missing_prompt() of the current name; empty when Finished.
    std::string prompt() const;

    // The reply to the question in the Prompt state; ignored in any other state. A line (a
    // trailing line feed is allowed) goes through parse_answer(): Yes moves to Create, No moves on
    // to the next file, Ask leaves everything as it is (the question is asked again). An empty
    // optional is the end of input: this file and every later one count as No, and the state is
    // Finished.
    void answer(const std::optional<std::string_view>& line);

    // The outcome of creating the current file in the Create state; ignored in any other state.
    // Records it and moves on to the next file.
    void created(bool ok);

    // One entry per file of the list, in order: true when the file was created.
    const std::vector<bool>& results() const { return results_; }

private:
    void advance();

    std::vector<MissingFile> missing_;
    std::vector<bool> results_;
    std::size_t index_ = 0;
    bool creating_ = false;
    bool finished_ = false;
};

} // namespace vermilion
