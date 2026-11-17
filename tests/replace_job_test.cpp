// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the Replace All job of the replace controller: slices, Cancel, tab change, no display.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtksourceview/gtksource.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/search_messages.h"
#include "ui/debounce_timer.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"
#include "ui/replace_controller.h"
#include "ui/search_controller.h"

namespace {

using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::take_string;
using vermilion::ui::DebounceTimer;
using vermilion::ui::ReplaceController;
using vermilion::ui::ReplaceJobEnd;
using vermilion::ui::ReplaceJobResult;
using vermilion::ui::ReplacePacing;
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

// The scheduler of the job: it fires when the test says so.
class FakeTimer : public DebounceTimer {
public:
    void start(unsigned delay_ms, std::function<void()> fire) override {
        delay = delay_ms;
        fire_ = std::move(fire);
    }

    void stop() override { fire_ = nullptr; }

    bool armed() const { return static_cast<bool>(fire_); }

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

    unsigned delay = 0;

private:
    std::function<void()> fire_;
};

// Keeps the "replace slice:" and "replace run:" debug messages of the "vermilion" log domain.
class LogCatcher {
public:
    LogCatcher() : handler_(g_log_set_handler("vermilion", G_LOG_LEVEL_DEBUG, keep, &lines_)) {}
    ~LogCatcher() { g_log_remove_handler("vermilion", handler_); }

    LogCatcher(const LogCatcher&) = delete;
    LogCatcher& operator=(const LogCatcher&) = delete;

    const std::vector<std::string>& slices() const { return lines_.slices; }
    const std::vector<std::string>& runs() const { return lines_.runs; }

private:
    struct Lines {
        std::vector<std::string> slices;
        std::vector<std::string> runs;
    };

    static void keep(const gchar*, GLogLevelFlags, const gchar* message, gpointer data) {
        const std::string text = message;
        Lines* lines = static_cast<Lines*>(data);
        if (text.rfind("replace slice: ", 0) == 0) {
            lines->slices.push_back(text);
        } else if (text.rfind("replace run: ", 0) == 0) {
            lines->runs.push_back(text);
        }
    }

    Lines lines_; // before handler_, which points at it
    guint handler_;
};

class ReplaceJobTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gtk_source_init(); }

    void SetUp() override {
        buffer_ = ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
        search_ = std::make_unique<SearchController>(std::make_unique<IdleTimer>());
        std::unique_ptr<FakeTimer> timer = std::make_unique<FakeTimer>();
        timer_ = timer.get();
        replace_ = std::make_unique<ReplaceController>(*search_, std::move(timer));
        replace_->set_on_job_progress([this]() { ++progress_calls_; });
        replace_->set_on_job_done(
            [this](const ReplaceJobResult& result) { done_.push_back(result); });
        ReplacePacing pacing;
        pacing.slice_ms = 0;
        pacing.gap_ms = 7;
        pacing.max_matches = 2;
        replace_->set_pacing(pacing);
    }

    GtkTextBuffer* text_buffer() const { return GTK_TEXT_BUFFER(buffer_.get()); }

    // Puts `text` into buffer_ (no undo history, cursor at the start) and searches it.
    void open(const std::string& text) {
        gtk_text_buffer_begin_irreversible_action(text_buffer());
        gtk_text_buffer_set_text(text_buffer(), text.c_str(), -1);
        gtk_text_buffer_end_irreversible_action(text_buffer());
        select(0, 0);
        search_->set_buffer(buffer_.get());
    }

    // Types `text` and presses Enter.
    void search(const std::string& text) {
        search_->set_text(text);
        search_->apply();
    }

    // Opens `text`, searches for `pattern`, sets the replacement and starts the job.
    ReplaceStart begin(const std::string& text, const std::string& pattern,
                       const std::string& replacement) {
        open(text);
        search(pattern);
        replace_->set_replacement(replacement);
        return replace_->start_replace_all();
    }

    // Runs the main loop until the search has counted `expected` matches (at most 5 s).
    void count_matches(int expected) {
        const gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
        while (search_->session()->count() != expected && g_get_monotonic_time() < deadline) {
            g_main_context_iteration(nullptr, FALSE);
            g_usleep(1000);
        }
        ASSERT_EQ(search_->session()->count(), expected);
    }

    // Fires the job timer until nothing waits; returns how many times it fired.
    int run_job() {
        int fired = 0;
        while (fired < 10000 && timer_->fire_now()) {
            ++fired;
        }
        return fired;
    }

    void select(int from, int to) {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &start, from);
        gtk_text_buffer_get_iter_at_offset(text_buffer(), &end, to);
        gtk_text_buffer_select_range(text_buffer(), &end, &start);
    }

    std::string text() const { return buffer_text(text_buffer()); }

    static std::string buffer_text(GtkTextBuffer* buffer) {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        return take_string(gtk_text_buffer_get_text(buffer, &start, &end, TRUE));
    }

    bool has_selection() const {
        GtkTextIter start;
        GtkTextIter end;
        return gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end) != FALSE;
    }

    int selection_start() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&start);
    }

    int selection_end() const {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_selection_bounds(text_buffer(), &start, &end);
        return gtk_text_iter_get_offset(&end);
    }

    // The offset of the cursor (the insert mark).
    int cursor() const {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(text_buffer(), &iter,
                                         gtk_text_buffer_get_insert(text_buffer()));
        return gtk_text_iter_get_offset(&iter);
    }

    bool can_undo() const { return gtk_text_buffer_get_can_undo(text_buffer()) != FALSE; }

    ObjectPtr<GtkSourceBuffer> buffer_;
    std::unique_ptr<SearchController> search_;
    FakeTimer* timer_ = nullptr; // borrowed; owned by replace_
    std::unique_ptr<ReplaceController> replace_;
    int progress_calls_ = 0;
    std::vector<ReplaceJobResult> done_;
};

TEST_F(ReplaceJobTest, AJobStartsWithTheGapAndHasReplacedNothingYet) {
    open("ab ab ab ab ab");
    search("ab");
    count_matches(5);
    replace_->set_replacement("X");
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    EXPECT_TRUE(replace_->job_active());
    EXPECT_TRUE(timer_->armed());
    EXPECT_EQ(timer_->delay, 7u);
    EXPECT_EQ(text(), "ab ab ab ab ab");
    EXPECT_EQ(replace_->job_replaced(), 0u);
    EXPECT_EQ(replace_->status(), "Replacing: 0 of about 5");
    EXPECT_FALSE(replace_->status_is_problem());
    EXPECT_TRUE(done_.empty());
}

TEST_F(ReplaceJobTest, SlicesReportProgressAndTheLastOneEndsTheJob) {
    open("ab ab ab ab ab");
    search("ab");
    count_matches(5);
    replace_->set_replacement("X");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_EQ(text(), "X X ab ab ab");
    EXPECT_EQ(replace_->job_replaced(), 2u);
    EXPECT_EQ(progress_calls_, 1);
    EXPECT_TRUE(done_.empty());
    EXPECT_TRUE(timer_->armed());
    EXPECT_EQ(timer_->delay, 7u);
    EXPECT_EQ(replace_->status(), "Replacing: 2 of about 5");
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_EQ(text(), "X X X X ab");
    EXPECT_EQ(progress_calls_, 2);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_FALSE(replace_->job_active());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(progress_calls_, 2);
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Finished);
    EXPECT_EQ(done_[0].count, 5u);
    EXPECT_FALSE(done_[0].in_selection);
    EXPECT_EQ(text(), "X X X X X");
    EXPECT_EQ(search_->status(), vermilion::replaced_message(5));
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(5));
    EXPECT_FALSE(replace_->status_is_problem());
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(ReplaceJobTest, ASliceThatStopsAtTheLimitIsFollowedByAnEmptyFinishingSlice) {
    ASSERT_EQ(begin("ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_TRUE(replace_->job_active());
    EXPECT_EQ(text(), "X X X X");
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_FALSE(replace_->job_active());
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].count, 4u);
    EXPECT_EQ(progress_calls_, 2);
}

TEST_F(ReplaceJobTest, CancelKeepsWhatWasReplacedAsOneUndoStep) {
    LogCatcher log;
    ASSERT_EQ(begin("ab ab ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_TRUE(timer_->fire_now());
    replace_->cancel_job();
    EXPECT_FALSE(replace_->job_active());
    EXPECT_FALSE(timer_->armed());
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Cancelled);
    EXPECT_EQ(done_[0].count, 4u);
    EXPECT_EQ(text(), "X X X X ab ab");
    EXPECT_EQ(search_->status(), vermilion::replace_stopped_message(4));
    EXPECT_EQ(replace_->status(), vermilion::replace_stopped_message(4));
    ASSERT_EQ(log.runs().size(), 1u);
    EXPECT_NE(log.runs()[0].find(" matches=4 end=cancelled "), std::string::npos);
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab ab ab ab");
    EXPECT_FALSE(can_undo());
}

TEST_F(ReplaceJobTest, CancelBeforeTheFirstSliceReplacesNothing) {
    ASSERT_EQ(begin("ab ab", "ab", "X"), ReplaceStart::Started);
    replace_->cancel_job();
    replace_->cancel_job();
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Cancelled);
    EXPECT_EQ(done_[0].count, 0u);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_EQ(search_->status(), vermilion::replace_stopped_message(0));
    EXPECT_FALSE(timer_->armed());
}

TEST_F(ReplaceJobTest, ChangingTheBufferEndsTheJobAndKeepsWhatWasReplaced) {
    LogCatcher log;
    ASSERT_EQ(begin("ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_EQ(text(), "X X ab ab");
    ObjectPtr<GtkSourceBuffer> other =
        ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    search_->set_buffer(other.get());
    EXPECT_FALSE(replace_->job_active());
    EXPECT_FALSE(timer_->armed());
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::BufferChanged);
    EXPECT_EQ(done_[0].count, 2u);
    EXPECT_EQ(text(), "X X ab ab");
    ASSERT_EQ(log.runs().size(), 1u);
    EXPECT_NE(log.runs()[0].find(" matches=2 end=buffer-changed "), std::string::npos);
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab ab");
}

TEST_F(ReplaceJobTest, DestroyingTheControllerEndsTheJobWithoutCallingBack) {
    ASSERT_EQ(begin("ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    replace_.reset();
    EXPECT_TRUE(done_.empty());
    EXPECT_EQ(text(), "X X ab ab");
    gtk_text_buffer_undo(text_buffer());
    EXPECT_EQ(text(), "ab ab ab ab");
    search_->set_buffer(nullptr);
    EXPECT_TRUE(done_.empty());
}

TEST_F(ReplaceJobTest, AWholeDocumentJobKeepsTheCursorAndLeavesNothingSelected) {
    open("ab cd ab");
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_offset(text_buffer(), &iter, 4);
    gtk_text_buffer_place_cursor(text_buffer(), &iter);
    search("ab");
    replace_->set_replacement("XYZ");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    run_job();
    EXPECT_EQ(text(), "XYZ cd XYZ");
    EXPECT_FALSE(has_selection());
    EXPECT_EQ(cursor(), 5);
}

TEST_F(ReplaceJobTest, ASelectionJobWorksInTheRangeAndLeavesItSelected) {
    LogCatcher log;
    open("ab ab ab ab");
    select(3, 8);
    replace_->capture_selection();
    ASSERT_TRUE(replace_->choose_scope(ReplaceScope::SelectionOnly));
    search("ab");
    replace_->set_replacement("X");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    EXPECT_EQ(replace_->status(), "Replacing: 0");
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_EQ(replace_->status(), "Replacing: 2");
    run_job();
    EXPECT_EQ(text(), "ab X X ab");
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Finished);
    EXPECT_EQ(done_[0].count, 2u);
    EXPECT_TRUE(done_[0].in_selection);
    EXPECT_EQ(selection_start(), 3);
    EXPECT_EQ(selection_end(), 6);
    EXPECT_EQ(search_->status(), vermilion::replaced_message(2) + " in the selection");
    ASSERT_EQ(log.runs().size(), 1u);
    EXPECT_NE(log.runs()[0].find(" scope=selection range=5 characters=11 "), std::string::npos);
}

TEST_F(ReplaceJobTest, ABadReplacementIsRefused) {
    open("ab ab");
    search_->set_text("(a)(b)");
    search_->set_regex(true);
    replace_->set_replacement("\\3");
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::Refused);
    EXPECT_FALSE(replace_->job_active());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(text(), "ab ab");
    EXPECT_TRUE(replace_->status_is_problem());
    EXPECT_TRUE(done_.empty());
}

TEST_F(ReplaceJobTest, WithoutABufferThereIsNothingToReplaceAndNothingIsShown) {
    search("ab");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::NothingToReplace);
    EXPECT_FALSE(replace_->job_active());
    EXPECT_EQ(replace_->status(), "");
}

TEST_F(ReplaceJobTest, AnEmptySearchTextSaysNothingToReplace) {
    open("ab ab");
    search("");
    replace_->set_replacement("X");
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::NothingToReplace);
    EXPECT_FALSE(replace_->job_active());
    EXPECT_FALSE(timer_->armed());
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(0));
    EXPECT_EQ(text(), "ab ab");
}

TEST_F(ReplaceJobTest, ATextWithNoMatchEndsAtTheFirstSlice) {
    open("ab ab");
    search("zz");
    count_matches(0);
    replace_->set_replacement("X");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    EXPECT_TRUE(timer_->fire_now());
    EXPECT_FALSE(replace_->job_active());
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Finished);
    EXPECT_EQ(done_[0].count, 0u);
    EXPECT_EQ(text(), "ab ab");
    EXPECT_EQ(replace_->status(), vermilion::replaced_message(0));
}

TEST_F(ReplaceJobTest, WhileAJobRunsASecondStartIsBusyAndReplaceDoesNothing) {
    ASSERT_EQ(begin("ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_EQ(replace_->start_replace_all(), ReplaceStart::Busy);
    EXPECT_TRUE(replace_->job_active());
    EXPECT_TRUE(timer_->armed());
    EXPECT_FALSE(replace_->replace().next.found);
    EXPECT_EQ(text(), "ab ab ab ab");
    run_job();
    EXPECT_EQ(text(), "X X X X");
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].count, 4u);
}

TEST_F(ReplaceJobTest, ANewJobCanStartAfterOneEnded) {
    ASSERT_EQ(begin("ab ab", "ab", "X"), ReplaceStart::Started);
    run_job();
    EXPECT_EQ(text(), "X X");
    search("X");
    replace_->set_replacement("Y");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    run_job();
    EXPECT_EQ(text(), "Y Y");
    EXPECT_EQ(done_.size(), 2u);
    EXPECT_EQ(search_->status(), vermilion::replaced_message(2));
}

TEST_F(ReplaceJobTest, ThePacingDefaultsHoldTheGapIsAtLeastOneAndATimeLimitedJobStillFinishes) {
    std::string lines;
    for (int i = 0; i < 3000; ++i) {
        lines += "ab\n";
    }
    open(lines);
    search("ab");
    const ReplacePacing defaults;
    EXPECT_EQ(defaults.slice_ms, 25u);
    EXPECT_EQ(defaults.gap_ms, 5u);
    EXPECT_EQ(defaults.max_matches, 0u);
    ReplacePacing pacing;
    pacing.slice_ms = 1;
    pacing.gap_ms = 0;
    pacing.max_matches = 0;
    replace_->set_pacing(pacing);
    EXPECT_EQ(replace_->pacing().slice_ms, 1u);
    replace_->set_replacement("X");
    ASSERT_EQ(replace_->start_replace_all(), ReplaceStart::Started);
    EXPECT_EQ(timer_->delay, 1u);
    run_job();
    ASSERT_EQ(done_.size(), 1u);
    EXPECT_EQ(done_[0].end, ReplaceJobEnd::Finished);
    EXPECT_EQ(done_[0].count, 3000u);
    std::string expected;
    for (int i = 0; i < 3000; ++i) {
        expected += "X\n";
    }
    EXPECT_EQ(text(), expected);
}

TEST_F(ReplaceJobTest, EverySliceAndTheEndAreLogged) {
    LogCatcher log;
    ASSERT_EQ(begin("ab ab ab ab ab", "ab", "X"), ReplaceStart::Started);
    EXPECT_EQ(run_job(), 3);
    ASSERT_EQ(log.slices().size(), 3u);
    EXPECT_EQ(log.slices()[0].rfind("replace slice: n=2 ms=", 0), 0u);
    EXPECT_NE(log.slices()[0].find(" done=2"), std::string::npos);
    EXPECT_NE(log.slices()[2].find(" done=5"), std::string::npos);
    ASSERT_EQ(log.runs().size(), 1u);
    const std::string& run = log.runs()[0];
    EXPECT_EQ(run.rfind("replace run: matches=5 end=finished ms=", 0), 0u);
    EXPECT_NE(run.find(" slices=3 scope=whole characters=14 "), std::string::npos);
    const std::size_t peak = run.find(" peak_kb=");
    ASSERT_NE(peak, std::string::npos);
    ASSERT_LT(peak + 9, run.size());
    EXPECT_TRUE(g_ascii_isdigit(run[peak + 9]));
}

} // namespace
