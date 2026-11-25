// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpSearch: the message after a search (no display needed) and, on a real display (the
// tests skip without one), the controls, the rows of the results and what a click does.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/manual_results.h"
#include "core/search_options.h"
#include "ui/help_search.h"

namespace {

using vermilion::PageResult;
using vermilion::SearchOptions;
using vermilion::ui::HelpSearch;
using vermilion::ui::results_message;

bool g_display = false;

PageResult page(const std::string& file, const std::string& title, std::size_t count,
                const std::string& snippet) {
    PageResult result;
    result.file = file;
    result.title = title;
    result.count = count;
    result.snippet = snippet;
    return result;
}

std::vector<PageResult> sample_results() {
    return {page("a.md", "Alpha", 3, "the first match of Alpha"),
            page("b.md", "Beta", 1204, "one in Beta")};
}

// The text of every label under `widget`, in order.
void collect_labels(GtkWidget* widget, std::vector<std::string>& texts) {
    if (GTK_IS_LABEL(widget)) {
        texts.push_back(gtk_label_get_text(GTK_LABEL(widget)));
    }
    for (GtkWidget* child = gtk_widget_get_first_child(widget); child != nullptr;
         child = gtk_widget_get_next_sibling(child)) {
        collect_labels(child, texts);
    }
}

// The first widget under `widget` (itself included) that `matches`, or null.
template <typename Match>
GtkWidget* find_widget(GtkWidget* widget, Match matches) {
    if (matches(widget)) {
        return widget;
    }
    for (GtkWidget* child = gtk_widget_get_first_child(widget); child != nullptr;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget* found = find_widget(child, matches);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

// The button with this label.
GtkWidget* find_button(GtkWidget* root, const char* label) {
    return find_widget(root, [label](GtkWidget* widget) {
        if (!GTK_IS_BUTTON(widget)) {
            return false;
        }
        const char* text = gtk_button_get_label(GTK_BUTTON(widget)); // borrowed
        return text != nullptr && std::string(text) == label;
    });
}

// The check box with this label.
GtkWidget* find_check(GtkWidget* root, const char* label) {
    return find_widget(root, [label](GtkWidget* widget) {
        if (!GTK_IS_CHECK_BUTTON(widget)) {
            return false;
        }
        const char* text = gtk_check_button_get_label(GTK_CHECK_BUTTON(widget)); // borrowed
        return text != nullptr && std::string(text) == label;
    });
}

// Runs the main loop for about `ms` milliseconds.
void spin(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

TEST(HelpSearchMessageTest, NoPageWithAMatchIsNotFound) {
    EXPECT_EQ(results_message({}), "Not found");
}

TEST(HelpSearchMessageTest, TheMessageCountsTheMatchesAndThePages) {
    EXPECT_EQ(results_message({page("a.md", "Alpha", 1, "")}), "1 match on 1 page");
    EXPECT_EQ(results_message({page("a.md", "Alpha", 2, "")}), "2 matches on 1 page");
    EXPECT_EQ(results_message(sample_results()), "1,207 matches on 2 pages");
}

class HelpSearchTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        window_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        search_ = std::make_unique<HelpSearch>(
            [this](const SearchOptions& options) { runs_.push_back(options); },
            [this](const std::string& file) { opened_.push_back(file); },
            [this](bool forward) { steps_.push_back(forward); });
        gtk_window_set_child(window_, search_->widget());
    }

    void TearDown() override {
        search_.reset();
        if (window_ != nullptr) {
            gtk_window_destroy(window_);
        }
    }

    // Shows the window and lets GTK build the rows.
    void present() {
        gtk_window_set_default_size(window_, 320, 400);
        gtk_window_present(window_);
        spin(300);
    }

    GtkWindow* window_ = nullptr;
    std::unique_ptr<HelpSearch> search_;
    std::vector<SearchOptions> runs_;
    std::vector<std::string> opened_;
    std::vector<bool> steps_;
};

TEST_F(HelpSearchTest, ItStartsWithAnEmptyBoxEveryOptionOffAndNoResults) {
    EXPECT_EQ(search_->options(), SearchOptions{});
    EXPECT_TRUE(search_->results().empty());
    EXPECT_TRUE(runs_.empty());
}

TEST_F(HelpSearchTest, TheOptionsAreWhatTheControlsHold) {
    SearchOptions wanted;
    wanted.text = "needle";
    wanted.match_case = true;
    wanted.regex = true;
    search_->set_options(wanted);
    EXPECT_EQ(search_->options(), wanted);
    EXPECT_TRUE(
        gtk_check_button_get_active(GTK_CHECK_BUTTON(find_check(search_->widget(), "Match case"))));
    EXPECT_FALSE(
        gtk_check_button_get_active(GTK_CHECK_BUTTON(find_check(search_->widget(), "Whole word"))));
    EXPECT_TRUE(
        gtk_check_button_get_active(GTK_CHECK_BUTTON(find_check(search_->widget(), "Regex"))));
    wanted.text.clear();
    wanted.match_case = false;
    wanted.whole_word = true;
    wanted.regex = false;
    search_->set_options(wanted);
    EXPECT_EQ(search_->options(), wanted);
    EXPECT_TRUE(runs_.empty()); // setting the controls does not search
}

TEST_F(HelpSearchTest, EnterInTheBoxRunsTheSearchWithTheOptions) {
    SearchOptions wanted;
    wanted.text = "needle";
    wanted.whole_word = true;
    search_->set_options(wanted);
    GtkWidget* box = find_widget(search_->widget(), [](GtkWidget* w) { return GTK_IS_ENTRY(w); });
    ASSERT_NE(box, nullptr);
    g_signal_emit_by_name(box, "activate");
    ASSERT_EQ(runs_.size(), std::size_t{1});
    EXPECT_EQ(runs_[0], wanted);
    search_->run();
    EXPECT_EQ(runs_.size(), std::size_t{2});
}

TEST_F(HelpSearchTest, ChangingTheBoxOrAnOptionDoesNotRunTheSearch) {
    GtkWidget* box = find_widget(search_->widget(), [](GtkWidget* w) { return GTK_IS_ENTRY(w); });
    ASSERT_NE(box, nullptr);
    gtk_editable_set_text(GTK_EDITABLE(box), "typed");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(find_check(search_->widget(), "Regex")), TRUE);
    EXPECT_TRUE(runs_.empty());
    EXPECT_EQ(search_->options().text, "typed");
    EXPECT_TRUE(search_->options().regex);
}

TEST_F(HelpSearchTest, PreviousAndNextCallTheStepHandler) {
    GtkWidget* previous = find_button(search_->widget(), "Previous");
    GtkWidget* next = find_button(search_->widget(), "Next");
    ASSERT_NE(previous, nullptr);
    ASSERT_NE(next, nullptr);
    g_signal_emit_by_name(next, "clicked");
    g_signal_emit_by_name(previous, "clicked");
    search_->step(true);
    ASSERT_EQ(steps_.size(), std::size_t{3});
    EXPECT_TRUE(steps_[0]);
    EXPECT_FALSE(steps_[1]);
    EXPECT_TRUE(steps_[2]);
}

TEST_F(HelpSearchTest, FocusTextSelectsTheTextOfTheBox) {
    // The reader may have switched off GTK's own "select the text when a box gets the focus".
    GtkSettings* settings = gtk_settings_get_default(); // borrowed
    gboolean select_on_focus = TRUE;
    g_object_get(settings, "gtk-entry-select-on-focus", &select_on_focus, nullptr);
    g_object_set(settings, "gtk-entry-select-on-focus", FALSE, nullptr);
    SearchOptions wanted;
    wanted.text = "needle";
    search_->set_options(wanted);
    present();
    search_->focus_text();
    GtkWidget* box = find_widget(search_->widget(), [](GtkWidget* w) { return GTK_IS_ENTRY(w); });
    ASSERT_NE(box, nullptr);
    GtkWidget* focus = gtk_window_get_focus(window_); // borrowed; the text inside the box
    ASSERT_NE(focus, nullptr);
    EXPECT_TRUE(focus == box || gtk_widget_is_ancestor(focus, box));
    int start = -1;
    int end = -1;
    EXPECT_TRUE(gtk_editable_get_selection_bounds(GTK_EDITABLE(box), &start, &end));
    EXPECT_EQ(start, 0);
    EXPECT_EQ(end, 6);
    g_object_set(settings, "gtk-entry-select-on-focus", select_on_focus, nullptr);
}

TEST_F(HelpSearchTest, TheListShowsOneRowForEveryPage) {
    search_->set_results(sample_results());
    EXPECT_EQ(search_->results().size(), std::size_t{2});
    present();
    std::vector<std::string> texts;
    collect_labels(search_->widget(), texts);
    for (const char* text :
         {"Alpha", "3", "the first match of Alpha", "Beta", "1,204", "one in Beta"}) {
        SCOPED_TRACE(text);
        EXPECT_NE(std::find(texts.begin(), texts.end(), text), texts.end());
    }
}

TEST_F(HelpSearchTest, SettingTheResultsAgainReplacesTheRows) {
    search_->set_results(sample_results());
    present();
    search_->set_results({page("c.md", "Gamma", 2, "in Gamma")});
    spin(100);
    std::vector<std::string> texts;
    collect_labels(search_->widget(), texts);
    EXPECT_NE(std::find(texts.begin(), texts.end(), "Gamma"), texts.end());
    EXPECT_EQ(std::find(texts.begin(), texts.end(), "Alpha"), texts.end());
    search_->set_results({});
    spin(100);
    texts.clear();
    collect_labels(search_->widget(), texts);
    EXPECT_EQ(std::find(texts.begin(), texts.end(), "Gamma"), texts.end());
    EXPECT_TRUE(search_->results().empty());
}

TEST_F(HelpSearchTest, AClickOnARowCallsTheOpenHandlerWithItsFile) {
    search_->set_results(sample_results());
    search_->activate_row(1);
    ASSERT_EQ(opened_.size(), std::size_t{1});
    EXPECT_EQ(opened_[0], "b.md");
    search_->activate_row(2); // past the last row
    EXPECT_EQ(opened_.size(), std::size_t{1});
}

TEST_F(HelpSearchTest, TheRealListViewCallsTheOpenHandlerWhenARowIsActivated) {
    search_->set_results(sample_results());
    present();
    GtkWidget* list =
        find_widget(search_->widget(), [](GtkWidget* w) { return GTK_IS_LIST_VIEW(w); });
    ASSERT_NE(list, nullptr);
    EXPECT_TRUE(gtk_widget_has_css_class(list, vermilion::ui::kHelpResultsClass));
    g_signal_emit_by_name(list, "activate", 0u);
    ASSERT_EQ(opened_.size(), std::size_t{1});
    EXPECT_EQ(opened_[0], "a.md");
}

} // namespace
