// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the search controller: pause, options, Find Next and the status line, no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/search_messages.h"
#include "core/search_options.h"
#include "ui/debounce_timer.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/search_controller.h"

namespace {

using vermilion::SearchOptions;
using vermilion::gtkx::ObjectPtr;
using vermilion::ui::DebounceTimer;
using vermilion::ui::FindResult;
using vermilion::ui::SearchController;
using vermilion::ui::SearchSession;

// A timer that fires when the test says so.
class FakeTimer : public DebounceTimer {
public:
    void start(unsigned delay_ms, std::function<void()> fire) override {
        ++starts;
        delay = delay_ms;
        fire_ = std::move(fire);
    }

    void stop() override { fire_ = nullptr; }

    bool armed() const { return static_cast<bool>(fire_); }

    void fire_now() {
        std::function<void()> fire = std::move(fire_);
        fire_ = nullptr;
        if (fire) {
            fire();
        }
    }

    int starts = 0;
    unsigned delay = 0;

private:
    std::function<void()> fire_;
};

// Runs the main loop until the status reads `expected`, for at most five seconds.
void wait_for_status(const SearchController& controller, const std::string& expected) {
    const gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (controller.status() != expected && g_get_monotonic_time() < deadline) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// Selects the characters from offset `from` to offset `to` (the cursor ends at `to`).
void select_range(GtkTextBuffer* buffer, int from, int to) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_offset(buffer, &start, from);
    gtk_text_buffer_get_iter_at_offset(buffer, &end, to);
    gtk_text_buffer_select_range(buffer, &end, &start);
}

int selection_start(GtkTextBuffer* buffer) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    return gtk_text_iter_get_offset(&start);
}

int selection_end(GtkTextBuffer* buffer) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    return gtk_text_iter_get_offset(&end);
}

class SearchControllerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
        std::unique_ptr<FakeTimer> timer = std::make_unique<FakeTimer>();
        timer_ = timer.get();
        controller_ = std::make_unique<SearchController>(std::move(timer));
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Puts `text` into buffer_ with the cursor at the start and makes it the searched buffer.
    void open(const char* text) {
        gtk_text_buffer_set_text(text_buffer(), text, -1);
        select_range(text_buffer(), 0, 0);
        controller_->set_buffer(buffer_.get());
    }

    // Types `text` and presses Enter.
    void search(const std::string& text) {
        controller_->set_text(text);
        controller_->apply();
    }

    int start() const { return selection_start(text_buffer()); }
    int end() const { return selection_end(text_buffer()); }

    ObjectPtr<GtkSourceBuffer> buffer_;
    FakeTimer* timer_ = nullptr; // borrowed; owned by controller_
    std::unique_ptr<SearchController> controller_;
};

TEST_F(SearchControllerTest, WithoutABufferThereIsNothingToFind) {
    search("ab");
    EXPECT_FALSE(controller_->has_buffer());
    EXPECT_EQ(controller_->session(), nullptr);
    EXPECT_FALSE(controller_->find_next().found);
    EXPECT_FALSE(controller_->find_previous().found);
    EXPECT_EQ(controller_->status(), "");
    EXPECT_FALSE(controller_->status_is_problem());
    EXPECT_EQ(controller_->options().text, "ab");
}

TEST_F(SearchControllerTest, TypedTextWaitsForTheTimer) {
    open("ab ab ab");
    controller_->set_text("ab");
    EXPECT_TRUE(controller_->pending());
    EXPECT_TRUE(timer_->armed());
    EXPECT_EQ(timer_->delay, 200u);
    EXPECT_EQ(SearchController::kDebounceMs, 200u);
    EXPECT_EQ(controller_->text(), "ab");
    EXPECT_TRUE(controller_->has_text());
    EXPECT_EQ(controller_->options().text, "");
    EXPECT_EQ(controller_->status(), "");
    timer_->fire_now();
    EXPECT_FALSE(controller_->pending());
    EXPECT_EQ(controller_->options().text, "ab");
    wait_for_status(*controller_, "3 matches");
    EXPECT_EQ(controller_->status(), "3 matches");
}

TEST_F(SearchControllerTest, EveryKeystrokeRestartsTheTimer) {
    open("ab ab ab");
    controller_->set_text("a");
    controller_->set_text("ab");
    EXPECT_EQ(timer_->starts, 2);
    EXPECT_TRUE(timer_->armed());
    EXPECT_EQ(controller_->options().text, "");
    timer_->fire_now();
    EXPECT_EQ(controller_->options().text, "ab");
    EXPECT_FALSE(timer_->armed());
}

TEST_F(SearchControllerTest, TypingTheAppliedTextBackCancelsThePause) {
    open("ab ab ab");
    search("ab");
    controller_->set_text("abc");
    EXPECT_TRUE(controller_->pending());
    EXPECT_TRUE(timer_->armed());
    controller_->set_text("ab");
    EXPECT_FALSE(controller_->pending());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(controller_->options().text, "ab");
}

TEST_F(SearchControllerTest, ApplyAppliesAtOnceAndStopsTheTimer) {
    open("ab ab ab");
    controller_->set_text("ab");
    EXPECT_TRUE(controller_->pending());
    controller_->apply();
    EXPECT_FALSE(controller_->pending());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(controller_->options().text, "ab");
}

TEST_F(SearchControllerTest, FindNextAppliesThePendingTextFirst) {
    open("ab ab ab");
    controller_->set_text("ab");
    const FindResult result = controller_->find_next();
    EXPECT_TRUE(result.found);
    EXPECT_FALSE(result.wrapped);
    EXPECT_FALSE(controller_->pending());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 2);
}

TEST_F(SearchControllerTest, CheckboxesApplyAtOnceTogetherWithTheTypedText) {
    open("Abc abc ABC");
    controller_->set_text("abc");
    controller_->set_match_case(true);
    EXPECT_FALSE(controller_->pending());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(controller_->options().text, "abc");
    EXPECT_TRUE(controller_->options().match_case);
    wait_for_status(*controller_, "1 match");
    EXPECT_EQ(controller_->status(), "1 match");
    controller_->set_match_case(false);
    EXPECT_FALSE(controller_->options().match_case);
    wait_for_status(*controller_, "3 matches");
    EXPECT_EQ(controller_->status(), "3 matches");
}

TEST_F(SearchControllerTest, WholeWordAndRegularExpressionAreAppliedToo) {
    open("abc abcd a1 b22");
    controller_->set_text("abc");
    controller_->set_whole_word(true);
    wait_for_status(*controller_, "1 match");
    EXPECT_EQ(controller_->status(), "1 match");
    controller_->set_whole_word(false);
    wait_for_status(*controller_, "2 matches");
    EXPECT_EQ(controller_->status(), "2 matches");
    controller_->set_text("\\d");
    controller_->set_regex(true);
    EXPECT_TRUE(controller_->options().regex);
    EXPECT_EQ(controller_->options().text, "\\d");
    wait_for_status(*controller_, "3 matches");
    EXPECT_EQ(controller_->status(), "3 matches");
}

TEST_F(SearchControllerTest, ASearchThatMatchesNothingSaysNotFound) {
    open("abc");
    search("zz");
    wait_for_status(*controller_, "Not found");
    EXPECT_EQ(controller_->status(), "Not found");
    EXPECT_FALSE(controller_->find_next().found);
    EXPECT_EQ(controller_->status(), "Not found");
}

TEST_F(SearchControllerTest, FindNextAndPreviousShowThePosition) {
    open("ab ab ab");
    search("ab");
    wait_for_status(*controller_, "3 matches");
    EXPECT_TRUE(controller_->find_next().found);
    EXPECT_EQ(controller_->status(), "1 of 3 matches");
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 2);
    EXPECT_TRUE(controller_->find_next().found);
    EXPECT_EQ(controller_->status(), "2 of 3 matches");
    EXPECT_EQ(start(), 3);
    EXPECT_EQ(end(), 5);
    EXPECT_TRUE(controller_->find_previous().found);
    EXPECT_EQ(controller_->status(), "1 of 3 matches");
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 2);
}

TEST_F(SearchControllerTest, AnUnchangedOptionDoesNotRestartTheScan) {
    open("ab ab ab");
    search("ab");
    wait_for_status(*controller_, "3 matches");
    controller_->set_match_case(false);
    controller_->set_whole_word(false);
    controller_->set_regex(false);
    controller_->apply();
    EXPECT_EQ(controller_->status(), "3 matches");
}

TEST_F(SearchControllerTest, WrappingIsReportedUntilTheNextFind) {
    open("ab ab ab");
    search("ab");
    wait_for_status(*controller_, "3 matches");
    select_range(text_buffer(), 6, 8);
    FindResult result = controller_->find_next();
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(controller_->status(), "Wrapped to the top");
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 2);
    result = controller_->find_next();
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(controller_->status(), "2 of 3 matches");
    select_range(text_buffer(), 0, 2);
    result = controller_->find_previous();
    EXPECT_TRUE(result.wrapped);
    EXPECT_EQ(controller_->status(), "Wrapped to the bottom");
    EXPECT_EQ(start(), 6);
    EXPECT_EQ(end(), 8);
    result = controller_->find_previous();
    EXPECT_FALSE(result.wrapped);
    EXPECT_EQ(controller_->status(), "2 of 3 matches");
}

TEST_F(SearchControllerTest, AnInvalidPatternIsAProblem) {
    open("a (b");
    controller_->set_text("(");
    controller_->set_regex(true);
    SearchOptions options;
    options.text = "(";
    options.regex = true;
    ASSERT_TRUE(vermilion::pattern_error(options).has_value());
    EXPECT_TRUE(controller_->status_is_problem());
    EXPECT_EQ(controller_->status(),
              vermilion::pattern_problem_message(*vermilion::pattern_error(options)));
    EXPECT_FALSE(controller_->find_next().found);
    EXPECT_TRUE(controller_->status_is_problem());
    controller_->set_regex(false);
    EXPECT_FALSE(controller_->status_is_problem());
    wait_for_status(*controller_, "1 match");
    EXPECT_EQ(controller_->status(), "1 match");
}

TEST_F(SearchControllerTest, EmptyTextClearsTheStatus) {
    open("ab");
    search("ab");
    wait_for_status(*controller_, "1 match");
    search("");
    EXPECT_EQ(controller_->status(), "");
    EXPECT_FALSE(controller_->has_text());
    EXPECT_FALSE(controller_->find_next().found);
    EXPECT_EQ(controller_->status(), "");
}

TEST_F(SearchControllerTest, TheStatusFollowsEditsOfTheBuffer) {
    open("ab");
    search("ab");
    wait_for_status(*controller_, "1 match");
    gtk_text_buffer_set_text(text_buffer(), "ab ab", -1);
    wait_for_status(*controller_, "2 matches");
    EXPECT_EQ(controller_->status(), "2 matches");
}

TEST_F(SearchControllerTest, ChangingTheBufferMovesTheSearch) {
    open("ab ab");
    search("ab");
    wait_for_status(*controller_, "2 matches");
    ObjectPtr<GtkSourceBuffer> other =
        ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    GtkTextBuffer* other_text = GTK_TEXT_BUFFER(other.get());
    gtk_text_buffer_set_text(other_text, "ab ab ab", -1);
    select_range(other_text, 0, 0);
    controller_->set_buffer(other.get());
    EXPECT_TRUE(controller_->has_buffer());
    wait_for_status(*controller_, "3 matches");
    EXPECT_EQ(controller_->status(), "3 matches");
    EXPECT_TRUE(controller_->find_next().found);
    EXPECT_EQ(selection_start(other_text), 0);
    EXPECT_EQ(selection_end(other_text), 2);
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 0);
    controller_->set_buffer(nullptr);
    EXPECT_FALSE(controller_->has_buffer());
    EXPECT_EQ(controller_->status(), "");
    EXPECT_EQ(controller_->options().text, "ab");
    EXPECT_FALSE(controller_->find_next().found);
    controller_->set_buffer(buffer_.get());
    wait_for_status(*controller_, "2 matches");
    EXPECT_EQ(controller_->status(), "2 matches");
}

TEST_F(SearchControllerTest, TheSameBufferKeepsTheSession) {
    open("ab ab");
    SearchSession* first = controller_->session();
    ASSERT_NE(first, nullptr);
    controller_->set_buffer(buffer_.get());
    EXPECT_EQ(controller_->session(), first);
}

TEST_F(SearchControllerTest, TheStatusCallbackRunsOnlyWhenTheStatusChanges) {
    int calls = 0;
    controller_->set_on_status_changed([&calls]() { ++calls; });
    open("ab ab ab");
    search("ab");
    wait_for_status(*controller_, "3 matches");
    const int before = calls;
    EXPECT_GT(before, 0);
    controller_->refresh_status();
    EXPECT_EQ(calls, before);
    EXPECT_TRUE(controller_->find_next().found);
    EXPECT_EQ(calls, before + 1);
}

TEST_F(SearchControllerTest, ShowMessageReplacesTheStatusUntilTheNextFind) {
    open("ab ab");
    search("ab");
    wait_for_status(*controller_, "2 matches");
    controller_->show_message("Replaced 3 matches");
    EXPECT_EQ(controller_->status(), "Replaced 3 matches");
    EXPECT_FALSE(controller_->status_is_problem());
    EXPECT_TRUE(controller_->find_next().found);
    EXPECT_EQ(controller_->status(), "1 of 2 matches");
    controller_->show_message("Note");
    controller_->set_match_case(true);
    EXPECT_NE(controller_->status(), "Note");
    wait_for_status(*controller_, "1 of 2 matches");
    controller_->show_message("Note");
    controller_->show_message("");
    EXPECT_EQ(controller_->status(), "1 of 2 matches");
}

TEST_F(SearchControllerTest, TheBufferHookRunsBeforeTheOldSessionGoes) {
    int calls = 0;
    bool had_session = false;
    GtkSourceBuffer* seen = nullptr;
    controller_->set_on_buffer_changing([&]() {
        ++calls;
        had_session = controller_->session() != nullptr;
        seen = controller_->buffer();
    });
    open("ab");
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(had_session);
    EXPECT_EQ(seen, nullptr);
    controller_->set_buffer(buffer_.get());
    EXPECT_EQ(calls, 1);
    ObjectPtr<GtkSourceBuffer> other =
        ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    controller_->set_buffer(other.get());
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(had_session);
    EXPECT_EQ(seen, buffer_.get());
    controller_->set_buffer(nullptr);
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(seen, other.get());
    controller_->set_on_buffer_changing(nullptr);
    controller_->set_buffer(buffer_.get());
    EXPECT_EQ(calls, 3);
}

TEST_F(SearchControllerTest, AFrozenStatusDoesNotChangeAndUnfreezingRefreshes) {
    open("ab ab");
    search("ab");
    wait_for_status(*controller_, "2 matches");
    int calls = 0;
    controller_->set_on_status_changed([&calls]() { ++calls; });
    controller_->set_status_frozen(true);
    controller_->show_message("Note");
    EXPECT_EQ(controller_->status(), "2 matches");
    EXPECT_EQ(calls, 0);
    controller_->set_status_frozen(true);
    controller_->set_status_frozen(false);
    EXPECT_EQ(controller_->status(), "Note");
    EXPECT_EQ(calls, 1);
    controller_->set_status_frozen(false);
    EXPECT_EQ(calls, 1);
    controller_->set_on_status_changed(nullptr);
}

TEST_F(SearchControllerTest, OptionsReadBackAsSet) {
    EXPECT_FALSE(controller_->options().match_case);
    EXPECT_FALSE(controller_->options().whole_word);
    EXPECT_FALSE(controller_->options().regex);
    controller_->set_match_case(true);
    controller_->set_whole_word(true);
    controller_->set_regex(true);
    EXPECT_TRUE(controller_->options().match_case);
    EXPECT_TRUE(controller_->options().whole_word);
    EXPECT_TRUE(controller_->options().regex);
    controller_->set_text("a");
    EXPECT_EQ(controller_->text(), "a");
    EXPECT_EQ(controller_->options().text, "");
    EXPECT_TRUE(controller_->pending());
}

} // namespace
