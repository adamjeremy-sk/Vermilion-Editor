// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the replace controller: Replace, Replace All, the scope and the status, no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/search_messages.h"
#include "core/search_options.h"
#include "core/search_replacement.h"
#include "ui/debounce_timer.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"
#include "ui/replace_controller.h"
#include "ui/search_controller.h"

namespace {

using vermilion::SearchOptions;
using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::take_string;
using vermilion::ui::DebounceTimer;
using vermilion::ui::ReplaceController;
using vermilion::ui::ReplaceJobResult;
using vermilion::ui::ReplaceResult;
using vermilion::ui::ReplaceScope;
using vermilion::ui::ReplaceStart;
using vermilion::ui::SearchController;

// A timer that never fires by itself: the tests apply the search text with apply().
class IdleTimer : public DebounceTimer {
public:
    void start(unsigned, std::function<void()> fire) override { fire_ = std::move(fire); }
    void stop() override { fire_ = nullptr; }

private:
    std::function<void()> fire_;
};

// The scheduler of the Replace All job: it fires when the test says so.
class FakeTimer : public DebounceTimer {
public:
    void start(unsigned, std::function<void()> fire) override { fire_ = std::move(fire); }
    void stop() override { fire_ = nullptr; }

    // Runs the call that waits; false when none waits.
    bool fire_now() {
        std::function<void()> fire = std::move(fire_);
        fire_ = nullptr;
        if (!fire) {
            return false;
        }
        fire();
        return true;
    }

private:
    std::function<void()> fire_;
};

// Runs the main loop until the status reads `expected`, for at most five seconds.
void wait_for_status(const ReplaceController& controller, const std::string& expected) {
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

std::string buffer_text(GtkTextBuffer* buffer) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return take_string(gtk_text_buffer_get_text(buffer, &start, &end, TRUE));
}

class ReplaceControllerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
        search_ = std::make_unique<SearchController>(std::make_unique<IdleTimer>());
        std::unique_ptr<FakeTimer> timer = std::make_unique<FakeTimer>();
        timer_ = timer.get();
        replace_ = std::make_unique<ReplaceController>(*search_, std::move(timer));
        replace_->set_on_job_done(
            [this](const ReplaceJobResult& result) { replaced_ = result.count; });
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Puts `text` into buffer_ with the cursor at the start and makes it the searched buffer.
    void open(const char* text) {
        gtk_text_buffer_set_text(text_buffer(), text, -1);
        select(0, 0);
        search_->set_buffer(buffer_.get());
    }

    void select(int from, int to) { select_range(text_buffer(), from, to); }

    // Types `text` and presses Enter.
    void search(const std::string& text) {
        search_->set_text(text);
        search_->apply();
    }

    std::string text() const { return buffer_text(text_buffer()); }

    int start() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&start);
    }

    int end() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&end);
    }

    // Replace All as the window does it, to the end: starts the job and fires its timer until it
    // is over. Returns the number of matches replaced; 0 when no job started.
    std::uint64_t replace_all_now() {
        replaced_ = 0;
        if (replace_->start_replace_all() != ReplaceStart::Started) {
            return 0;
        }
        int fired = 0;
        while (fired < 10000 && timer_->fire_now()) {
            ++fired;
        }
        EXPECT_FALSE(replace_->job_active());
        return replaced_;
    }

    ObjectPtr<GtkSourceBuffer> buffer_;
    std::unique_ptr<SearchController> search_;
    FakeTimer* timer_ = nullptr; // borrowed; owned by replace_
    std::unique_ptr<ReplaceController> replace_;
    std::uint64_t replaced_ = 0; // what the last job reported
};

TEST_F(ReplaceControllerTest, WithoutABufferNothingHappens) {
    search("ab");
    replace_->set_replacement("x");
    ReplaceResult one = replace_->replace();
    EXPECT_FALSE(one.refused);
    EXPECT_FALSE(one.replaced);
    EXPECT_FALSE(one.next.found);
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::NothingToReplace);
    EXPECT_FALSE(replace_->job_active());
    replace_->capture_selection();
    EXPECT_FALSE(replace_->selection_available());
    EXPECT_FALSE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::WholeDocument));
    EXPECT_EQ(replace_->scope(), ReplaceScope::WholeDocument);
    EXPECT_EQ(replace_->status(), "");
    EXPECT_FALSE(replace_->status_is_problem());
}

TEST_F(ReplaceControllerTest, ReplaceReplacesTheSelectedMatchAndSelectsTheNext) {
    open("ab ab ab");
    search("ab");
    search_->find_next();
    replace_->set_replacement("xyz");
    ReplaceResult result = replace_->replace();
    EXPECT_FALSE(result.refused);
    EXPECT_TRUE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_EQ(text(), "xyz ab ab");
    EXPECT_EQ(start(), 4);
    EXPECT_EQ(end(), 6);
    EXPECT_EQ(replace_->replacement(), "xyz");
}

TEST_F(ReplaceControllerTest, ReplaceWithoutASelectedMatchOnlySelectsTheNext) {
    open("ab ab");
    search("ab");
    replace_->set_replacement("x");
    ReplaceResult result = replace_->replace();
    EXPECT_FALSE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(end(), 2);
}

TEST_F(ReplaceControllerTest, ReplaceAppliesTheTypedTextFirst) {
    open("ab ab");
    search_->set_text("ab");
    EXPECT_TRUE(search_->pending());
    ReplaceResult result = replace_->replace();
    EXPECT_FALSE(search_->pending());
    EXPECT_TRUE(result.next.found);
    EXPECT_EQ(start(), 0);
}

TEST_F(ReplaceControllerTest, ReplacingTheLastMatchSaysSo) {
    open("x ab");
    search("ab");
    search_->find_next();
    replace_->set_replacement("c");
    ReplaceResult result = replace_->replace();
    EXPECT_TRUE(result.replaced);
    EXPECT_FALSE(result.next.found);
    EXPECT_EQ(text(), "x c");
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(1));
    EXPECT_FALSE(replace_->status_is_problem());
}

TEST_F(ReplaceControllerTest, ReplaceReportsAWrap) {
    open("ab cd ab");
    search("ab");
    search_->find_next();
    search_->find_next();
    EXPECT_EQ(start(), 6);
    replace_->set_replacement("X");
    ReplaceResult result = replace_->replace();
    EXPECT_TRUE(result.replaced);
    EXPECT_TRUE(result.next.found);
    EXPECT_TRUE(result.next.wrapped);
    EXPECT_EQ(text(), "ab cd X");
    EXPECT_EQ(start(), 0);
    EXPECT_EQ(replace_->status(), vermilion::wrapped_message(true));
}

TEST_F(ReplaceControllerTest, ReplaceUsesGroupsInARegularExpression) {
    open("john smith");
    search_->set_text("(\\w+) (\\w+)");
    search_->set_regex(true);
    search_->find_next();
    replace_->set_replacement("\\2 \\1");
    ReplaceResult result = replace_->replace();
    EXPECT_TRUE(result.replaced);
    EXPECT_EQ(text(), "smith john");
}

TEST_F(ReplaceControllerTest, ABadReplacementIsRefusedAndShownWhileTyping) {
    open("ab ab");
    search_->set_text("(a)(b)");
    search_->set_regex(true);
    replace_->set_replacement("\\3");
    SearchOptions options = search_->options();
    std::string expected =
        vermilion::replacement_problem_message(*vermilion::replacement_error(options, "\\3"));
    EXPECT_EQ(replace_->status(), expected);
    EXPECT_TRUE(replace_->status_is_problem());
    EXPECT_TRUE(replace_->replace().refused);
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::Refused);
    EXPECT_EQ(text(), "ab ab");
    replace_->set_replacement("\\1\\2");
    EXPECT_FALSE(replace_->status_is_problem());
    wait_for_status(*replace_, "2 matches");
    EXPECT_EQ(replace_->status(), "2 matches");
}

TEST_F(ReplaceControllerTest, ABackslashIsFineWhenRegularExpressionIsOff) {
    open("ab");
    search("ab");
    replace_->set_replacement("\\3");
    EXPECT_FALSE(replace_->status_is_problem());
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "\\3");
}

TEST_F(ReplaceControllerTest, ABadPatternComesBeforeABadReplacement) {
    open("ab");
    search_->set_text("(");
    search_->set_regex(true);
    replace_->set_replacement("\\3");
    EXPECT_TRUE(search_->status_is_problem());
    EXPECT_EQ(replace_->status(), search_->status());
    EXPECT_TRUE(replace_->status_is_problem());
}

TEST_F(ReplaceControllerTest, TheStatusIsTheSearchStatusWithoutAProblem) {
    open("ab ab ab");
    search("ab");
    wait_for_status(*replace_, "3 matches");
    EXPECT_EQ(replace_->status(), "3 matches");
    EXPECT_FALSE(replace_->status_is_problem());
}

TEST_F(ReplaceControllerTest, ReplaceAllReplacesEveryMatch) {
    open("ab ab ab");
    search("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 3u);
    EXPECT_EQ(text(), "X X X");
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(3));
}

TEST_F(ReplaceControllerTest, ReplaceAllWithNothingToReplace) {
    open("ab ab");
    search("zz");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 0u);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(0));
}

TEST_F(ReplaceControllerTest, ReplaceAllAppliesTheTypedTextFirst) {
    open("ab ab");
    search_->set_text("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 2u);
    EXPECT_FALSE(search_->pending());
}

TEST_F(ReplaceControllerTest, NoSelectionMeansNoSelectionScope) {
    open("ab ab");
    replace_->capture_selection();
    EXPECT_FALSE(replace_->selection_available());
    EXPECT_EQ(replace_->scope(), ReplaceScope::WholeDocument);
    EXPECT_FALSE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    EXPECT_EQ(replace_->scope(), ReplaceScope::WholeDocument);
}

TEST_F(ReplaceControllerTest, AOneLineSelectionIsAvailableButNotPreselected) {
    open("ab ab ab");
    select(3, 5);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->selection_available());
    EXPECT_EQ(replace_->scope(), ReplaceScope::WholeDocument);
}

TEST_F(ReplaceControllerTest, AMultiLineSelectionPreselectsTheSelectionScope) {
    open("a\nb\nc");
    select(0, 3);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->selection_available());
    EXPECT_EQ(replace_->scope(), ReplaceScope::SelectionOnly);
}

TEST_F(ReplaceControllerTest, ReplaceAllInTheSelectionKeepsTheRangeWhenTheSelectionMoves) {
    open("ab ab ab ab");
    select(3, 8);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    search_->find_next();
    EXPECT_EQ(start(), 9);
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 2u);
    EXPECT_EQ(text(), "ab X X ab");
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(2) + " in the selection");
}

TEST_F(ReplaceControllerTest, TheRangeFollowsTheEditsThatReplaceMakes) {
    open("ab ab ab ab");
    select(0, 5);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("XYZ");
    select(0, 2);
    EXPECT_TRUE(replace_->replace().replaced);
    EXPECT_EQ(text(), "XYZ ab ab ab");
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "XYZ XYZ ab ab");
}

TEST_F(ReplaceControllerTest, AReplacementAtTheStartOfTheRangeStaysInsideIt) {
    open("ab ab ab");
    select(0, 5);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("XYZ");
    select(0, 2);
    EXPECT_TRUE(replace_->replace().replaced);
    EXPECT_EQ(text(), "XYZ ab ab");
    search("XYZ");
    replace_->set_replacement("Q");
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "Q ab ab");
}

TEST_F(ReplaceControllerTest, AReplacementAtTheEndOfTheRangeStaysInsideIt) {
    open("ab ab ab");
    select(0, 5);
    replace_->capture_selection();
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("XYZ");
    select(3, 5);
    EXPECT_TRUE(replace_->replace().replaced);
    EXPECT_EQ(text(), "ab XYZ ab");
    search("XYZ");
    replace_->set_replacement("Q");
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "ab Q ab");
}

TEST_F(ReplaceControllerTest, ChoosingTheScopeTakesTheCurrentSelection) {
    open("ab ab ab");
    select(0, 2);
    replace_->capture_selection();
    select(6, 8);
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "ab ab X");
}

TEST_F(ReplaceControllerTest, ChoosingTheScopeWithoutASelectionKeepsTheRange) {
    open("ab ab ab");
    select(3, 5);
    replace_->capture_selection();
    select(0, 0);
    EXPECT_TRUE(replace_->selection_available());
    EXPECT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 1u);
    EXPECT_EQ(text(), "ab X ab");
}

TEST_F(ReplaceControllerTest, CapturingTheSelectionAgainForgetsTheOldRange) {
    open("ab ab ab");
    select(3, 5);
    replace_->capture_selection();
    select(0, 0);
    replace_->capture_selection();
    EXPECT_FALSE(replace_->selection_available());
    EXPECT_FALSE(replace_->choose_scope(ReplaceScope::SelectionOnly));
}

TEST_F(ReplaceControllerTest, AnotherBufferDropsTheRange) {
    open("ab\nab\nab");
    select(0, 4);
    replace_->capture_selection();
    EXPECT_EQ(replace_->scope(), ReplaceScope::SelectionOnly);
    ObjectPtr<GtkSourceBuffer> other =
        ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(other.get()), "ab ab", -1);
    select_range(GTK_TEXT_BUFFER(other.get()), 0, 0);
    search_->set_buffer(other.get());
    EXPECT_FALSE(replace_->selection_available());
    EXPECT_EQ(replace_->scope(), ReplaceScope::WholeDocument);
    search("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_all_now(), 2u);
    EXPECT_EQ(buffer_text(GTK_TEXT_BUFFER(other.get())), "X X");
}

} // namespace
