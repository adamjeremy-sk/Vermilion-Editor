// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the start-up question's walk: a fake window, the real store, a real main loop.

#include <gtest/gtest.h>

#include <glib.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/recovery_file.h"
#include "core/recovery_store.h"
#include "ui/recovery_prompt.h"

namespace {

namespace fs = std::filesystem;
using vermilion::DocumentId;
using vermilion::RecoveryHeader;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using vermilion::ui::RecoveryPrompt;
using Strings = std::vector<std::string>;

const std::string kA = "aaaaaaaa-0000-4000-8000-000000000001";
const std::string kB = "bbbbbbbb-0000-4000-8000-000000000002";
const std::string kC = "cccccccc-0000-4000-8000-000000000003";
const std::string kD = "dddddddd-0000-4000-8000-000000000004";
const char* const kKept1 =
    "message 1 recovery copy was kept. It is offered again at the next start.";
const char* const kKept2 =
    "message 2 recovery copies were kept. They are offered again at the next start.";

// The window the prompt talks to: it records what it is asked and keeps each question until the
// test answers it.
struct Fake {
    struct Question {
        std::string message;
        std::string detail;
        Strings buttons;
        std::function<void(int)> on_answer;
    };

    Strings events; // close_blank, add, adopt and message, in order
    std::vector<RecoveryPrompt::Document> docs;
    std::vector<Question> asked;
    std::optional<Question> pending;
    bool adopt_ok = true;
    bool path_kept = true; // add_document reports the path when the copy has one
    DocumentId last_id = 0;

    RecoveryPrompt::Host host() {
        RecoveryPrompt::Host host;
        host.ask = [this](const std::string& message, const std::string& detail,
                          const Strings& buttons, std::function<void(int)> on_answer) {
            asked.push_back({message, detail, buttons, on_answer});
            pending = asked.back();
        };
        host.close_blank = [this] { events.push_back("close_blank"); };
        host.add_document = [this](RecoveryPrompt::Document document) {
            events.push_back("add " + document.path.value_or("untitled") + " " + document.text);
            docs.push_back(document);
            RecoveryPrompt::Added added;
            added.id = ++last_id;
            added.has_path = document.path.has_value() && path_kept;
            return added;
        };
        host.adopt = [this](DocumentId id, const std::string& copy,
                            std::optional<std::int64_t> mtime) {
            events.push_back("adopt " + std::to_string(id) + " " + copy + " " +
                             (mtime ? std::to_string(*mtime) : "-"));
            return adopt_ok;
        };
        host.message = [this](const std::string& text) { events.push_back("message " + text); };
        return host;
    }

    // Answers the waiting question with the button of this label.
    void answer(const std::string& label) {
        ASSERT_TRUE(pending.has_value());
        Question question = *pending;
        pending.reset();
        auto button = std::find(question.buttons.begin(), question.buttons.end(), label);
        ASSERT_TRUE(button != question.buttons.end());
        question.on_answer(static_cast<int>(button - question.buttons.begin()));
    }

    // Answers with the last button, as Escape does.
    void answer_last() {
        ASSERT_TRUE(pending.has_value());
        Question question = *pending;
        pending.reset();
        question.on_answer(static_cast<int>(question.buttons.size()) - 1);
    }
};

std::string make_root() {
    g_autofree char* made = g_dir_make_tmp("vermilion-prompt-XXXXXX", nullptr);
    return made != nullptr ? made : "";
}

class PromptTest : public ::testing::Test {
protected:
    ~PromptTest() override {
        prompt.reset();
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }

    void put(const std::string& id, const RecoveryHeader& header, const std::string& text) {
        ASSERT_TRUE(store.write(id, header, text).is_ok());
    }

    // A file in the temporary folder, and its modified time as the system reports it.
    std::string make_file(const std::string& name) {
        std::string path = root + "/" + name;
        std::ofstream(path) << "x";
        return path;
    }

    static std::int64_t time_of(const std::string& path) {
        struct stat st {};
        stat(path.c_str(), &st);
        return static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000 + st.st_mtim.tv_nsec / 1000;
    }

    static RecoveryHeader untitled(const std::string& title, std::int64_t copied) {
        RecoveryHeader header;
        header.title = title;
        header.copied_us = copied;
        return header;
    }

    static RecoveryHeader of_file(const std::string& path, std::int64_t copied,
                                  std::optional<std::int64_t> mtime) {
        RecoveryHeader header;
        g_autofree char* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
        header.uri = uri;
        header.copied_us = copied;
        header.file_mtime_us = mtime;
        return header;
    }

    // A file that is not a copy, under the name of a copy.
    void put_damaged(const std::string& id) {
        fs::create_directories(store.folder());
        std::ofstream(store.path_of(id)) << "this is not a recovery copy\n";
    }

    bool exists(const std::string& id) const { return fs::exists(store.path_of(id)); }

    void start() {
        prompt = std::make_unique<RecoveryPrompt>(store, fake.host());
        prompt->start();
    }

    // Runs the main loop until the condition holds, or fails after five seconds.
    bool wait(const std::function<bool()>& condition) {
        gint64 limit = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
        while (!condition() && g_get_monotonic_time() < limit) {
            g_main_context_iteration(nullptr, FALSE);
            g_usleep(500);
        }
        return condition();
    }

    bool wait_question() {
        return wait([&] { return fake.pending.has_value(); });
    }

    bool wait_finished() {
        return wait([&] { return prompt->finished(); });
    }

    // Runs the main loop for a while: what is going to happen has happened.
    void settle() {
        gint64 limit = g_get_monotonic_time() + 300 * 1000;
        while (g_get_monotonic_time() < limit) {
            g_main_context_iteration(nullptr, FALSE);
            g_usleep(500);
        }
    }

    static bool has(const std::string& text, const char* part) {
        return text.find(part) != std::string::npos;
    }

    std::string root = make_root();
    RecoveryStore store{root, RecoveryProgram::Light};
    Fake fake;
    std::unique_ptr<RecoveryPrompt> prompt; // last: it goes first
};

TEST_F(PromptTest, NothingIsAskedWhenThereAreNoCopies) {
    start();
    ASSERT_TRUE(wait_finished());
    EXPECT_TRUE(fake.asked.empty());
    EXPECT_TRUE(fake.events.empty());
}

TEST_F(PromptTest, TheQuestionNamesTheCopyAndOffersTheButtons) {
    put(kA, untitled("Untitled 2", 100), "hello");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[0].message, "Untitled 2"));
    EXPECT_FALSE(has(fake.asked[0].message, " of "));
    EXPECT_EQ((Strings{"Recover", "Discard", "Decide Later"}), fake.asked[0].buttons);
    EXPECT_FALSE(prompt->finished());
}

TEST_F(PromptTest, CopiesAreAskedInOrderAndRecoverAllIsOfferedWhileTwoRemain) {
    put(kB, untitled("Untitled 2", 200), "second");
    put(kA, untitled("Untitled 1", 100), "first");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[0].message, "Untitled 1"));
    EXPECT_TRUE(has(fake.asked[0].message, "(1 of 2)"));
    EXPECT_EQ((Strings{"Recover", "Discard", "Recover All", "Decide Later"}),
              fake.asked[0].buttons);
    fake.answer("Decide Later");
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[1].message, "Untitled 2"));
    EXPECT_TRUE(has(fake.asked[1].message, "(2 of 2)"));
    EXPECT_EQ((Strings{"Recover", "Discard", "Decide Later"}), fake.asked[1].buttons);
    fake.answer("Decide Later");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{kKept2}), fake.events);
}

TEST_F(PromptTest, StartingASecondTimeDoesNothing) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    prompt->start();
    settle();
    EXPECT_EQ(1u, fake.asked.size());
}

TEST_F(PromptTest, RecoverOpensTheFileAndTakesOverTheCopy) {
    std::string path = make_file("notes.txt");
    std::int64_t mtime = time_of(path);
    put(kA, of_file(path, 100, mtime), "recovered text");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[0].message, "notes.txt"));
    EXPECT_TRUE(has(fake.asked[0].detail, "has not changed"));
    fake.answer("Recover");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{"close_blank", "add " + path + " recovered text",
                       "adopt 1 " + kA + " " + std::to_string(mtime)}),
              fake.events);
    ASSERT_EQ(1u, fake.docs.size());
    EXPECT_TRUE(fake.docs[0].file_mtime_us == std::optional<std::int64_t>(mtime));
    EXPECT_TRUE(exists(kA));
}

TEST_F(PromptTest, AnUntitledCopyOpensWithItsFormatAndNoPath) {
    RecoveryHeader header = untitled("Untitled 3", 100);
    header.format.encoding = vermilion::TextEncoding::Windows1252;
    header.format.line_ending = vermilion::LineEnding::CrLf;
    header.mixed_line_endings = true;
    put(kA, header, "text");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    ASSERT_TRUE(wait_finished());
    ASSERT_EQ(1u, fake.docs.size());
    EXPECT_FALSE(fake.docs[0].path.has_value());
    EXPECT_FALSE(fake.docs[0].file_mtime_us.has_value());
    EXPECT_TRUE(fake.docs[0].format.encoding == vermilion::TextEncoding::Windows1252);
    EXPECT_TRUE(fake.docs[0].format.line_ending == vermilion::LineEnding::CrLf);
    EXPECT_TRUE(fake.docs[0].mixed_line_endings);
    EXPECT_EQ("text", fake.docs[0].text);
    EXPECT_EQ((Strings{"close_blank", "add untitled text", "adopt 1 " + kA + " -"}), fake.events);
}

TEST_F(PromptTest, TheBlankDocumentIsClosedOnceBeforeTheFirstDocument) {
    put(kA, untitled("Untitled 1", 100), "one");
    put(kB, untitled("Untitled 2", 200), "two");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{"close_blank", "add untitled one", "adopt 1 " + kA + " -",
                       "add untitled two", "adopt 2 " + kB + " -"}),
              fake.events);
}

TEST_F(PromptTest, ANeverRecoveredCopyLeavesTheBlankDocumentAlone) {
    put(kA, untitled("Untitled 1", 100), "one");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Discard");
    ASSERT_TRUE(wait_question());
    fake.answer("Delete");
    ASSERT_TRUE(wait_finished());
    EXPECT_TRUE(fake.events.empty());
}

TEST_F(PromptTest, AFileThatIsAlreadyOpenBecomesAnUntitledDocumentAndSaysSo) {
    std::string path = make_file("notes.txt");
    put(kA, of_file(path, 100, time_of(path)), "text");
    fake.path_kept = false;
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    ASSERT_TRUE(wait_finished());
    ASSERT_EQ(4u, fake.events.size());
    EXPECT_EQ("close_blank", fake.events[0]);
    EXPECT_EQ("add " + path + " text", fake.events[1]);
    EXPECT_TRUE(has(fake.events[2], "adopt 1 "));
    EXPECT_TRUE(has(fake.events[3], "message Recovered the text of "));
    EXPECT_TRUE(has(fake.events[3], "notes.txt"));
    EXPECT_TRUE(has(fake.events[3], "as an untitled document, because that file is already open."));
}

TEST_F(PromptTest, ACopyThatIsNotTakenOverIsKept) {
    put(kA, untitled("Untitled 1", 100), "text");
    fake.adopt_ok = false;
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{"close_blank", "add untitled text", "adopt 1 " + kA + " -", kKept1}),
              fake.events);
    EXPECT_TRUE(exists(kA));
}

TEST_F(PromptTest, DecideLaterAndEscapeKeepTheCopy) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_EQ("Decide Later", fake.asked[0].buttons.back());
    fake.answer_last();
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{kKept1}), fake.events);
    EXPECT_TRUE(exists(kA));
}

TEST_F(PromptTest, DiscardAsksBeforeItDeletes) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Discard");
    ASSERT_TRUE(wait_question());
    EXPECT_EQ("Delete this copy?", fake.asked[1].message);
    EXPECT_TRUE(has(fake.asked[1].detail, "cannot be undone"));
    EXPECT_EQ((Strings{"Delete", "Cancel"}), fake.asked[1].buttons);
    EXPECT_TRUE(exists(kA));
    fake.answer("Delete");
    ASSERT_TRUE(wait_finished());
    EXPECT_FALSE(exists(kA));
    EXPECT_TRUE(fake.events.empty());
}

TEST_F(PromptTest, CancelOnTheDeleteQuestionAsksTheFirstQuestionAgain) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Discard");
    ASSERT_TRUE(wait_question());
    fake.answer("Cancel");
    ASSERT_TRUE(wait_question());
    ASSERT_EQ(3u, fake.asked.size());
    EXPECT_EQ(fake.asked[0].message, fake.asked[2].message);
    EXPECT_EQ(fake.asked[0].detail, fake.asked[2].detail);
    EXPECT_EQ(fake.asked[0].buttons, fake.asked[2].buttons);
    EXPECT_TRUE(exists(kA));
    fake.answer("Decide Later");
    ASSERT_TRUE(wait_finished());
    EXPECT_TRUE(exists(kA));
}

TEST_F(PromptTest, ADamagedCopyOffersOnlyDiscardAndDecideLater) {
    put_damaged(kA);
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_EQ("A recovery copy is damaged.", fake.asked[0].message);
    EXPECT_EQ((Strings{"Discard", "Decide Later"}), fake.asked[0].buttons);
    fake.answer("Discard");
    ASSERT_TRUE(wait_question());
    fake.answer("Delete");
    ASSERT_TRUE(wait_finished());
    EXPECT_FALSE(exists(kA));
    EXPECT_TRUE(fake.events.empty());
}

TEST_F(PromptTest, ACopyWhoseTextIsDamagedIsShownAsDamaged) {
    put(kA, untitled("Untitled 1", 100), std::string("ab\0cd", 5));
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_EQ("Recover", fake.asked[0].buttons[0]);
    fake.answer("Recover");
    ASSERT_TRUE(wait_question());
    EXPECT_EQ("A recovery copy is damaged.", fake.asked[1].message);
    EXPECT_EQ((Strings{"Discard", "Decide Later"}), fake.asked[1].buttons);
    EXPECT_TRUE(fake.events.empty());
    fake.answer("Decide Later");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{kKept1}), fake.events);
    EXPECT_TRUE(exists(kA));
}

TEST_F(PromptTest, RecoverAllRecoversTheCopiesInOrderWithoutQuestions) {
    put(kC, untitled("Untitled 3", 300), "three");
    put(kA, untitled("Untitled 1", 100), "one");
    put(kB, untitled("Untitled 2", 200), "two");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover All");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ(1u, fake.asked.size());
    EXPECT_EQ(
        (Strings{"close_blank", "add untitled one", "adopt 1 " + kA + " -", "add untitled two",
                 "adopt 2 " + kB + " -", "add untitled three", "adopt 3 " + kC + " -"}),
        fake.events);
}

TEST_F(PromptTest, RecoverAllKeepsWhatItCannotRecoverAndCountsIt) {
    put(kA, untitled("Untitled 1", 100), "good one");
    put(kB, untitled("Untitled 2", 200), std::string("a\0b", 3));
    put(kC, untitled("Untitled 3", 300), "good two");
    put_damaged(kD);
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover All");
    ASSERT_TRUE(wait_finished());
    EXPECT_EQ((Strings{"close_blank", "add untitled good one", "adopt 1 " + kA + " -",
                       "add untitled good two", "adopt 2 " + kC + " -", kKept2}),
              fake.events);
    EXPECT_TRUE(exists(kB));
    EXPECT_TRUE(exists(kD));
}

TEST_F(PromptTest, TheQuestionSaysWhenTheFileHasChangedSinceTheCopy) {
    std::string path = make_file("notes.txt");
    put(kA, of_file(path, 100, time_of(path) - 5000000), "text");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[0].detail, "was last changed"));
}

TEST_F(PromptTest, TheQuestionSaysWhenTheFileIsGone) {
    put(kA, of_file(root + "/gone.txt", 100, 1794450001123456), "text");
    start();
    ASSERT_TRUE(wait_question());
    EXPECT_TRUE(has(fake.asked[0].detail, "no longer exists"));
}

TEST_F(PromptTest, NothingHappensAfterThePromptIsDestroyedWhileAQuestionIsOpen) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    prompt.reset();
    fake.answer("Recover");
    settle();
    EXPECT_TRUE(fake.events.empty());
    EXPECT_EQ(1u, fake.asked.size());
}

TEST_F(PromptTest, NothingHappensAfterThePromptIsDestroyedWhileACopyIsRead) {
    put(kA, untitled("Untitled 1", 100), "text");
    start();
    ASSERT_TRUE(wait_question());
    fake.answer("Recover");
    prompt.reset();
    settle();
    EXPECT_TRUE(fake.events.empty());
    EXPECT_EQ(1u, fake.asked.size());
}

TEST_F(PromptTest, NothingHappensAfterThePromptIsDestroyedWhileAFileTimeIsRead) {
    std::string path = make_file("notes.txt");
    put(kA, of_file(path, 100, time_of(path)), "text");
    start();
    prompt.reset();
    settle();
    EXPECT_TRUE(fake.asked.empty());
    EXPECT_TRUE(fake.events.empty());
}

} // namespace
