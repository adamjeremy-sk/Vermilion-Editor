// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the Files tab on a real display (the tests skip without one): the check box and the
// number box, what is stored back, the range of the number box, and when the page says that the
// person changed something.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstdint>
#include <memory>

#include "core/editor_settings.h"
#include "core/settings_keys.h"
#include "ui/settings_page_files.h"

namespace {

using vermilion::EditorSettings;
using vermilion::ui::FilesSettingsPage;

bool g_display = false;

class FilesSettingsPageTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    // The page is made here, after the display check: GTK widgets need a display. A window holds
    // its widget so that nothing is left floating.
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        window_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        page_ = std::make_unique<FilesSettingsPage>();
        gtk_window_set_child(window_, page_->widget());
        page_->set_changed([this] { ++changes_; });
    }

    void TearDown() override {
        page_.reset();
        if (window_ != nullptr) {
            gtk_window_destroy(window_);
        }
    }

    GtkSpinButton* spin() const { return GTK_SPIN_BUTTON(page_->limit_spin()); }
    bool confirm_active() const {
        return gtk_check_button_get_active(GTK_CHECK_BUTTON(page_->confirm_check())) != FALSE;
    }
    EditorSettings stored(const EditorSettings& base = EditorSettings{}) const {
        EditorSettings copy = base;
        page_->store(copy);
        return copy;
    }

    GtkWindow* window_ = nullptr;
    std::unique_ptr<FilesSettingsPage> page_;
    int changes_ = 0;
};

TEST_F(FilesSettingsPageTest, TheNumberBoxRunsFromTheLowestToTheHighestLimit) {
    GtkAdjustment* adjustment = gtk_spin_button_get_adjustment(spin());
    EXPECT_EQ(gtk_adjustment_get_lower(adjustment),
              static_cast<double>(vermilion::settings::kLongLineLimitMin));
    EXPECT_EQ(gtk_adjustment_get_upper(adjustment),
              static_cast<double>(vermilion::settings::kLongLineLimitMax));
    EXPECT_EQ(gtk_spin_button_get_digits(spin()), 0u);
}

TEST_F(FilesSettingsPageTest, LoadShowsTheCheckBox) {
    EditorSettings settings;
    settings.confirm_large_reload = false;
    page_->load(settings);
    EXPECT_FALSE(confirm_active());
    settings.confirm_large_reload = true;
    page_->load(settings);
    EXPECT_TRUE(confirm_active());
}

TEST_F(FilesSettingsPageTest, LoadShowsTheLimit) {
    EditorSettings settings;
    settings.long_line_limit = 20000;
    page_->load(settings);
    EXPECT_EQ(gtk_spin_button_get_value_as_int(spin()), 20000);
    settings.long_line_limit = 5000000;
    page_->load(settings);
    EXPECT_EQ(gtk_spin_button_get_value_as_int(spin()), 5000000);
}

TEST_F(FilesSettingsPageTest, TheDefaultsAreShown) {
    page_->load(vermilion::default_settings());
    EXPECT_TRUE(confirm_active());
    EXPECT_EQ(gtk_spin_button_get_value_as_int(spin()), vermilion::settings::kLongLineLimitDefault);
}

TEST_F(FilesSettingsPageTest, StoreRoundTripsWhatWasLoaded) {
    EditorSettings settings;
    settings.confirm_large_reload = false;
    settings.long_line_limit = 123000;
    page_->load(settings);
    const EditorSettings back = stored();
    EXPECT_FALSE(back.confirm_large_reload);
    EXPECT_EQ(back.long_line_limit, std::uint64_t{123000});
}

TEST_F(FilesSettingsPageTest, ChangesAreStored) {
    page_->load(EditorSettings{});
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page_->confirm_check()), FALSE);
    gtk_spin_button_set_value(spin(), 40000);
    const EditorSettings back = stored();
    EXPECT_FALSE(back.confirm_large_reload);
    EXPECT_EQ(back.long_line_limit, std::uint64_t{40000});
}

TEST_F(FilesSettingsPageTest, ALimitOutsideTheRangeIsMovedToTheNearestEnd) {
    page_->load(EditorSettings{});
    gtk_spin_button_set_value(spin(), 5.0);
    EXPECT_EQ(stored().long_line_limit,
              static_cast<std::uint64_t>(vermilion::settings::kLongLineLimitMin));
    gtk_spin_button_set_value(spin(), 90000000.0);
    EXPECT_EQ(stored().long_line_limit,
              static_cast<std::uint64_t>(vermilion::settings::kLongLineLimitMax));
}

TEST_F(FilesSettingsPageTest, StoreLeavesTheOtherFieldsAlone) {
    EditorSettings base;
    base.highlight_current_line = false;
    base.font = "Foo 9";
    base.tab_width = 8;
    page_->load(base);
    gtk_spin_button_set_value(spin(), 40000);
    EditorSettings back = stored(base);
    EXPECT_EQ(back.long_line_limit, std::uint64_t{40000});
    back.long_line_limit = base.long_line_limit;
    back.confirm_large_reload = base.confirm_large_reload;
    EXPECT_TRUE(back == base);
}

TEST_F(FilesSettingsPageTest, LoadingIsNotAChange) {
    EditorSettings settings;
    settings.confirm_large_reload = false;
    settings.long_line_limit = 20000;
    page_->load(settings);
    page_->load(EditorSettings{});
    EXPECT_EQ(changes_, 0);
}

TEST_F(FilesSettingsPageTest, EveryControlTellsWhenThePersonChangesIt) {
    page_->load(EditorSettings{});
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page_->confirm_check()), FALSE);
    const int after_box = changes_;
    EXPECT_GE(after_box, 1);
    gtk_spin_button_set_value(spin(), 40000);
    EXPECT_GT(changes_, after_box);
}

} // namespace
