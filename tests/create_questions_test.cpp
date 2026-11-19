// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for CreateQuestions: answers, repeated questions, failures and the end of input.

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/create_questions.h"
#include "core/open_arguments.h"

namespace {

using vermilion::CreateQuestions;
using vermilion::MissingFile;
using vermilion::QuestionAction;
using Line = std::optional<std::string_view>;
using Flags = std::vector<bool>;

std::vector<MissingFile> three() {
    return {MissingFile{"a.txt", "/x/a.txt", 0}, MissingFile{"b.txt", "/x/b.txt", 1},
            MissingFile{"c.txt", "/x/c.txt", 1}};
}

TEST(CreateQuestionsTest, NothingToAskIsFinishedAtOnce) {
    CreateQuestions questions({});
    EXPECT_EQ(questions.action(), QuestionAction::Finished);
    EXPECT_TRUE(questions.current() == nullptr);
    EXPECT_TRUE(questions.prompt().empty());
    EXPECT_TRUE(questions.results().empty());
}

TEST(CreateQuestionsTest, ItStartsWithTheFirstName) {
    CreateQuestions questions(three());
    EXPECT_EQ(questions.action(), QuestionAction::Prompt);
    ASSERT_TRUE(questions.current() != nullptr);
    EXPECT_EQ(questions.current()->path, std::string("/x/a.txt"));
    EXPECT_EQ(questions.prompt(), std::string("'a.txt' does not exist. Create it? (Y/n)"));
    EXPECT_EQ(questions.results(), (Flags{false, false, false}));
}

TEST(CreateQuestionsTest, YesAsksForTheCreationAndTheOutcomeMovesOn) {
    CreateQuestions questions(three());
    questions.answer(Line("y\n"));
    EXPECT_EQ(questions.action(), QuestionAction::Create);
    EXPECT_EQ(questions.current()->name, std::string("a.txt"));
    questions.created(true);
    EXPECT_EQ(questions.action(), QuestionAction::Prompt);
    EXPECT_EQ(questions.current()->name, std::string("b.txt"));
    EXPECT_EQ(questions.results(), (Flags{true, false, false}));
}

TEST(CreateQuestionsTest, AnEmptyLineMeansYes) {
    CreateQuestions questions(three());
    questions.answer(Line("\n"));
    EXPECT_EQ(questions.action(), QuestionAction::Create);
}

TEST(CreateQuestionsTest, NoMovesOnWithoutCreating) {
    CreateQuestions questions(three());
    questions.answer(Line("n"));
    EXPECT_EQ(questions.action(), QuestionAction::Prompt);
    EXPECT_EQ(questions.current()->name, std::string("b.txt"));
    EXPECT_EQ(questions.results(), (Flags{false, false, false}));
}

TEST(CreateQuestionsTest, JunkAsksTheSameQuestionAgain) {
    CreateQuestions questions(three());
    questions.answer(Line("maybe\n"));
    EXPECT_EQ(questions.action(), QuestionAction::Prompt);
    EXPECT_EQ(questions.current()->name, std::string("a.txt"));
    questions.answer(Line("NO\n"));
    EXPECT_EQ(questions.current()->name, std::string("b.txt"));
}

TEST(CreateQuestionsTest, AFailedCreationIsRecordedAsNotCreated) {
    CreateQuestions questions(three());
    questions.answer(Line("y"));
    questions.created(false);
    EXPECT_EQ(questions.current()->name, std::string("b.txt"));
    EXPECT_EQ(questions.results(), (Flags{false, false, false}));
}

TEST(CreateQuestionsTest, TheLastAnswerFinishes) {
    CreateQuestions questions(three());
    questions.answer(Line("y"));
    questions.created(true);
    questions.answer(Line("n"));
    questions.answer(Line("y"));
    EXPECT_EQ(questions.action(), QuestionAction::Create);
    questions.created(true);
    EXPECT_EQ(questions.action(), QuestionAction::Finished);
    EXPECT_TRUE(questions.current() == nullptr);
    EXPECT_TRUE(questions.prompt().empty());
    EXPECT_EQ(questions.results(), (Flags{true, false, true}));
}

TEST(CreateQuestionsTest, TheEndOfInputAnswersNoToEverythingThatIsLeft) {
    CreateQuestions questions(three());
    questions.answer(Line("y"));
    questions.created(true);
    questions.answer(std::nullopt);
    EXPECT_EQ(questions.action(), QuestionAction::Finished);
    EXPECT_EQ(questions.results(), (Flags{true, false, false}));
}

TEST(CreateQuestionsTest, AnswersInTheWrongStateAreIgnored) {
    CreateQuestions questions(three());
    questions.created(true);
    EXPECT_EQ(questions.action(), QuestionAction::Prompt);
    EXPECT_EQ(questions.results(), (Flags{false, false, false}));
    questions.answer(Line("y"));
    questions.answer(Line("n"));
    EXPECT_EQ(questions.action(), QuestionAction::Create);
    EXPECT_EQ(questions.current()->name, std::string("a.txt"));
    questions.answer(std::nullopt);
    EXPECT_EQ(questions.action(), QuestionAction::Create);
    questions.created(true);
    EXPECT_EQ(questions.current()->name, std::string("b.txt"));
}

TEST(CreateQuestionsTest, NothingChangesAfterTheEnd) {
    CreateQuestions questions(three());
    questions.answer(std::nullopt);
    questions.answer(Line("y"));
    questions.created(true);
    EXPECT_EQ(questions.action(), QuestionAction::Finished);
    EXPECT_EQ(questions.results(), (Flags{false, false, false}));
}

TEST(CreateQuestionsTest, ResultsFeedPathsToOpen) {
    std::vector<MissingFile> missing = three();
    CreateQuestions questions(missing);
    questions.answer(Line("y"));
    questions.created(true);
    questions.answer(Line("n"));
    questions.answer(Line("y"));
    questions.created(true);
    vermilion::OpenRequest request;
    request.paths = {"/x/p"};
    request.missing = missing;
    EXPECT_EQ(vermilion::paths_to_open(request, questions.results()),
              (std::vector<std::string>{"/x/a.txt", "/x/p", "/x/c.txt"}));
}

} // namespace
