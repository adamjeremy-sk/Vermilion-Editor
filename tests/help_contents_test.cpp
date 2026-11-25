// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpContents on a real display (the tests skip without one): the rows of the tree, the
// topic of the current page open and selected, and what a click on a row does.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/manual_outline.h"
#include "ui/help_contents.h"

namespace {

using vermilion::OutlineHeading;
using vermilion::OutlineTopic;
using vermilion::ui::HelpContents;

bool g_display = false;

// Three topics: Alpha with three headings (one of level 3), Beta with one, Gamma with none.
std::vector<OutlineTopic> sample_outline() {
    OutlineTopic alpha;
    alpha.file = "a.md";
    alpha.title = "Alpha";
    alpha.headings = {OutlineHeading{"One", 2, 0}, OutlineHeading{"Two", 2, 3},
                      OutlineHeading{"Deep", 3, 5}};
    OutlineTopic beta;
    beta.file = "b.md";
    beta.title = "Beta";
    beta.headings = {OutlineHeading{"Only", 2, 0}};
    OutlineTopic gamma;
    gamma.file = "c.md";
    gamma.title = "Gamma";
    return {alpha, beta, gamma};
}

// The titles of the rows on the screen, joined with "|"; a heading row starts with "-", a topic
// row that is open with "+".
std::string shown(const HelpContents& contents) {
    std::string text;
    for (const HelpContents::Row& row : contents.rows()) {
        if (!text.empty()) {
            text += "|";
        }
        text += row.heading ? "-" : (row.open ? "+" : "");
        text += row.title;
    }
    return text;
}

class HelpContentsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        window_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        contents_ = std::make_unique<HelpContents>(
            [this](const std::string& file) { topics_.push_back(file); },
            [this](const std::string& file, std::size_t index) {
                headings_.emplace_back(file, index);
            });
        gtk_window_set_child(window_, contents_->widget());
    }

    void TearDown() override {
        contents_.reset();
        if (window_ != nullptr) {
            gtk_window_destroy(window_);
        }
    }

    GtkWindow* window_ = nullptr;
    std::unique_ptr<HelpContents> contents_;
    std::vector<std::string> topics_;
    std::vector<std::pair<std::string, std::size_t>> headings_;
};

TEST_F(HelpContentsTest, UntilTheOutlineArrivesItSaysLoading) {
    EXPECT_TRUE(contents_->loading());
    EXPECT_TRUE(contents_->rows().empty());
    EXPECT_FALSE(contents_->selected().has_value());
    EXPECT_FALSE(contents_->set_topic_open("a.md", true));
    contents_->activate_row(0);
    EXPECT_TRUE(topics_.empty());
    contents_->set_outline(sample_outline());
    EXPECT_FALSE(contents_->loading());
    EXPECT_EQ(shown(*contents_), "Alpha|Beta|Gamma"); // every topic closed, none selected
    EXPECT_FALSE(contents_->selected().has_value());
}

TEST_F(HelpContentsTest, TheTopicOfTheCurrentPageIsOpenAndSelected) {
    contents_->set_outline(sample_outline());
    contents_->set_current("b.md");
    EXPECT_EQ(shown(*contents_), "Alpha|+Beta|-Only|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(1));
    contents_->set_current("a.md");
    EXPECT_EQ(shown(*contents_), "+Alpha|-One|-Two|-Deep|Beta|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(0));
    EXPECT_EQ(contents_->current(), "a.md");
}

TEST_F(HelpContentsTest, MovingOpensTheNewTopicAndClosesTheOneTheTreeOpened) {
    contents_->set_outline(sample_outline());
    contents_->set_current("a.md");
    contents_->set_current("b.md");
    EXPECT_EQ(shown(*contents_), "Alpha|+Beta|-Only|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(1));
    contents_->set_current("c.md"); // no headings: nothing to open
    EXPECT_EQ(shown(*contents_), "Alpha|Beta|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(2));
}

TEST_F(HelpContentsTest, ATopicThatTheReaderOpenedStaysOpen) {
    contents_->set_outline(sample_outline());
    EXPECT_FALSE(contents_->set_topic_open("c.md", true)); // no headings to show
    EXPECT_FALSE(contents_->set_topic_open("no-such-topic.md", true));
    ASSERT_TRUE(contents_->set_topic_open("a.md", true));
    EXPECT_EQ(shown(*contents_), "+Alpha|-One|-Two|-Deep|Beta|Gamma");
    contents_->set_current("a.md"); // already open: the tree did not open it
    contents_->set_current("b.md");
    EXPECT_EQ(shown(*contents_), "+Alpha|-One|-Two|-Deep|+Beta|-Only|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(4));
    contents_->set_current("c.md"); // Beta was opened by the tree, Alpha by the reader
    EXPECT_EQ(shown(*contents_), "+Alpha|-One|-Two|-Deep|Beta|Gamma");
    ASSERT_TRUE(contents_->set_topic_open("a.md", false));
    EXPECT_EQ(shown(*contents_), "Alpha|Beta|Gamma");
}

TEST_F(HelpContentsTest, ACurrentPageSetBeforeTheOutlineIsUsedWhenItArrives) {
    contents_->set_current("b.md");
    EXPECT_EQ(contents_->current(), "b.md");
    EXPECT_FALSE(contents_->selected().has_value());
    contents_->set_outline(sample_outline());
    EXPECT_EQ(shown(*contents_), "Alpha|+Beta|-Only|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(1));
}

TEST_F(HelpContentsTest, AFileThatIsNotATopicSelectsNothing) {
    contents_->set_outline(sample_outline());
    contents_->set_current("b.md");
    ASSERT_TRUE(contents_->selected().has_value());
    contents_->set_current("no-such-topic.md");
    EXPECT_FALSE(contents_->selected().has_value());
    EXPECT_EQ(shown(*contents_), "Alpha|+Beta|-Only|Gamma"); // nothing is closed or opened
}

TEST_F(HelpContentsTest, AClickOnATopicRowCallsTheTopicHandler) {
    contents_->set_outline(sample_outline());
    contents_->activate_row(1);
    ASSERT_EQ(topics_.size(), std::size_t{1});
    EXPECT_EQ(topics_[0], "b.md");
    EXPECT_TRUE(headings_.empty());
    contents_->activate_row(2);
    ASSERT_EQ(topics_.size(), std::size_t{2});
    EXPECT_EQ(topics_[1], "c.md");
    contents_->activate_row(3); // past the last row
    EXPECT_EQ(topics_.size(), std::size_t{2});
}

TEST_F(HelpContentsTest, AClickOnAHeadingRowCallsTheHeadingHandlerAndTheSelectionComesBack) {
    contents_->set_outline(sample_outline());
    contents_->set_current("a.md");
    ASSERT_EQ(shown(*contents_), "+Alpha|-One|-Two|-Deep|Beta|Gamma");
    contents_->activate_row(2);
    ASSERT_EQ(headings_.size(), std::size_t{1});
    EXPECT_EQ(headings_[0].first, "a.md");
    EXPECT_EQ(headings_[0].second, std::size_t{3}); // the heading's index in its page
    EXPECT_TRUE(topics_.empty());
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(0)); // the row of the page
}

TEST_F(HelpContentsTest, SettingTheOutlineAgainReplacesTheTree) {
    contents_->set_outline(sample_outline());
    contents_->set_current("b.md");
    std::vector<OutlineTopic> smaller = sample_outline();
    smaller.erase(smaller.begin());
    contents_->set_outline(smaller);
    EXPECT_EQ(shown(*contents_), "+Beta|-Only|Gamma");
    EXPECT_EQ(contents_->selected(), std::optional<std::size_t>(0));
    contents_->set_outline({});
    EXPECT_TRUE(contents_->rows().empty());
    EXPECT_FALSE(contents_->selected().has_value());
    EXPECT_FALSE(contents_->loading());
}

} // namespace
