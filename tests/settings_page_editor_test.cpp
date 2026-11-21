// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the Editor tab on a real display (the tests skip without one): the tab width in the
// drop-down, what is stored back, and when the page says that the person changed something.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <memory>

#include "core/editor_settings.h"
#include "ui/settings_page_editor.h"

namespace {

using vermilion::EditorSettings;
using vermilion::ui::EditorSettingsPage;

bool g_display = false;

class EditorSettingsPageTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    // The page is made here, after the display check: GTK widgets need a display. A window holds
    // its widget so that nothing is left floating.
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        window_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        page_ = std::make_unique<EditorSettingsPage>();
        gtk_window_set_child(window_, page_->widget());
        page_->set_changed([this] { ++changes_; });
    }

    void TearDown() override {
        page_.reset();
        if (window_ != nullptr) {
            gtk_window_destroy(window_);
        }
    }

    unsigned selected() const {
        return gtk_drop_down_get_selected(GTK_DROP_DOWN(page_->tab_width_dropdown()));
    }
    void select(unsigned index) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(page_->tab_width_dropdown()), index);
    }

    GtkWindow* window_ = nullptr;
    std::unique_ptr<EditorSettingsPage> page_;
    int changes_ = 0;
};

TEST(EditorSettingsPageIndex, FourIsFirstAndEightIsSecond) {
    EXPECT_EQ(EditorSettingsPage::tab_width_index(4), 0u);
    EXPECT_EQ(EditorSettingsPage::tab_width_index(8), 1u);
    EXPECT_EQ(EditorSettingsPage::tab_width_at(0), 4);
    EXPECT_EQ(EditorSettingsPage::tab_width_at(1), 8);
}

TEST(EditorSettingsPageIndex, AnythingElseGivesFour) {
    EXPECT_EQ(EditorSettingsPage::tab_width_index(0), 0u);
    EXPECT_EQ(EditorSettingsPage::tab_width_index(5), 0u);
    EXPECT_EQ(EditorSettingsPage::tab_width_index(-8), 0u);
    EXPECT_EQ(EditorSettingsPage::tab_width_at(2), 4);
    EXPECT_EQ(EditorSettingsPage::tab_width_at(GTK_INVALID_LIST_POSITION), 4);
}

TEST_F(EditorSettingsPageTest, LoadShowsTheTabWidth) {
    EditorSettings settings;
    settings.tab_width = 8;
    page_->load(settings);
    EXPECT_EQ(selected(), 1u);
    settings.tab_width = 4;
    page_->load(settings);
    EXPECT_EQ(selected(), 0u);
}

TEST_F(EditorSettingsPageTest, TheDefaultsShowFourSpaces) {
    page_->load(vermilion::default_settings());
    EXPECT_EQ(selected(), 0u);
}

TEST_F(EditorSettingsPageTest, ChoosingAWidthIsStored) {
    page_->load(EditorSettings{});
    select(1);
    EditorSettings back;
    page_->store(back);
    EXPECT_EQ(back.tab_width, 8);
    select(0);
    page_->store(back);
    EXPECT_EQ(back.tab_width, 4);
}

TEST_F(EditorSettingsPageTest, StoreLeavesTheOtherFieldsAlone) {
    EditorSettings base;
    base.highlight_current_line = false;
    base.font = "Foo 9";
    base.confirm_large_reload = false;
    base.long_line_limit = 20000;
    page_->load(base);
    select(1);
    EditorSettings back = base;
    page_->store(back);
    EXPECT_EQ(back.tab_width, 8);
    back.tab_width = base.tab_width;
    EXPECT_TRUE(back == base);
}

TEST_F(EditorSettingsPageTest, LoadingIsNotAChange) {
    EditorSettings settings;
    settings.tab_width = 8;
    page_->load(settings);
    settings.tab_width = 4;
    page_->load(settings);
    EXPECT_EQ(changes_, 0);
}

TEST_F(EditorSettingsPageTest, ChoosingAWidthTellsTheWindow) {
    page_->load(EditorSettings{});
    select(1);
    EXPECT_GE(changes_, 1);
}

} // namespace
