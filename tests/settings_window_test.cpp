// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the Settings window shell on a real display (the tests skip without one): the working
// copy, the buttons, what reaches the host, and what a hidden and shown window starts from. A fake
// page with one check box stands for the real pages.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/editor_settings.h"
#include "ui/settings_page.h"
#include "ui/settings_window.h"

namespace {

using vermilion::EditorSettings;
using vermilion::ui::SettingsPage;
using vermilion::ui::SettingsWindow;

bool g_display = false;

// A page with one check box for the field highlight_current_line and one text box for tab_width
// (the text "8" means 8, any other text means 5). The tests reach its controls through the pointers
// that the window leaves them with.
class FakePage : public SettingsPage {
public:
    FakePage() {
        box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); // floating; the notebook takes it over
        check_ = gtk_check_button_new_with_label("Highlight");
        entry_ = gtk_entry_new();
        gtk_box_append(GTK_BOX(box_), check_);
        gtk_box_append(GTK_BOX(box_), entry_);
        signals_.push_back(vermilion::gtkx::connect<void(GtkCheckButton*)>(
            check_, "toggled", [this](GtkCheckButton*) { notify_changed(); }));
        signals_.push_back(vermilion::gtkx::connect<void(GtkEditable*)>(
            entry_, "changed", [this](GtkEditable*) { notify_changed(); }));
    }

    GtkWidget* widget() const override { return box_; }

    void store(EditorSettings& settings) const override {
        settings.highlight_current_line = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_));
        settings.tab_width =
            std::string(gtk_editable_get_text(GTK_EDITABLE(entry_))) == "8" ? 8 : 5;
    }

    void toggle() {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check_),
                                    !gtk_check_button_get_active(GTK_CHECK_BUTTON(check_)));
    }
    void type(const std::string& text) {
        gtk_editable_set_text(GTK_EDITABLE(entry_), text.c_str());
    }
    bool checked() const { return gtk_check_button_get_active(GTK_CHECK_BUTTON(check_)); }
    std::string typed() const { return gtk_editable_get_text(GTK_EDITABLE(entry_)); }

protected:
    void show_settings(const EditorSettings& settings) override {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check_), settings.highlight_current_line);
        gtk_editable_set_text(GTK_EDITABLE(entry_), std::to_string(settings.tab_width).c_str());
    }

private:
    GtkWidget* box_ = nullptr;
    GtkWidget* check_ = nullptr;
    GtkWidget* entry_ = nullptr;
    std::vector<vermilion::gtkx::SignalConnection> signals_;
};

class SettingsWindowTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    // The window and the parent are made here, after the display check: GTK widgets need a display.
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        file_.highlight_current_line = true;
        file_.font = "Mono 9";
        file_.tab_width = 8;
        parent_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        SettingsWindow::Host host;
        host.current = [this] {
            ++reads_;
            return file_;
        };
        host.apply = [this](const EditorSettings& settings) { applied_.push_back(settings); };
        host.closed = [this] { ++closed_; };
        window_ = std::make_unique<SettingsWindow>(parent_, std::move(host));
        auto page = std::make_unique<FakePage>();
        page_ = page.get(); // borrowed; the window owns the page
        window_->add_page("Fake", std::move(page));
    }

    void TearDown() override {
        window_.reset();
        if (parent_ != nullptr) {
            gtk_window_destroy(parent_);
        }
    }

    static void click(GtkWidget* button) { g_signal_emit_by_name(button, "clicked"); }

    bool apply_enabled() const {
        return gtk_widget_get_sensitive(window_->apply_button()) != FALSE;
    }

    GtkWindow* parent_ = nullptr;
    std::unique_ptr<SettingsWindow> window_;
    FakePage* page_ = nullptr;
    EditorSettings file_;
    std::vector<EditorSettings> applied_;
    int reads_ = 0;
    int closed_ = 0;
};

TEST_F(SettingsWindowTest, ItIsHiddenModalAndTransientAtFirst) {
    EXPECT_FALSE(window_->is_open());
    EXPECT_TRUE(gtk_window_get_modal(window_->window()));
    EXPECT_EQ(gtk_window_get_transient_for(window_->window()), parent_);
    EXPECT_EQ(std::string(gtk_window_get_title(window_->window())), std::string("Settings"));
    EXPECT_EQ(window_->page_count(), 1);
    EXPECT_FALSE(apply_enabled());
    EXPECT_EQ(reads_, 0);
}

TEST_F(SettingsWindowTest, OpenShowsTheSettingsOfTheFileInThePage) {
    window_->open();
    EXPECT_TRUE(window_->is_open());
    EXPECT_EQ(reads_, 1);
    EXPECT_TRUE(page_->checked());
    EXPECT_EQ(page_->typed(), std::string("8"));
    EXPECT_FALSE(apply_enabled());
    EXPECT_TRUE(window_->working() == file_);
}

TEST_F(SettingsWindowTest, LoadingThePageIsNotAChange) {
    file_.highlight_current_line = false;
    window_->open();
    EXPECT_FALSE(page_->checked());
    EXPECT_FALSE(apply_enabled());
}

TEST_F(SettingsWindowTest, APageSaysNothingWhileItLoadsAndSaysSoWhenTheUserChangesIt) {
    FakePage lone;
    int told = 0;
    lone.set_changed([&told] { ++told; });
    EditorSettings settings;
    settings.highlight_current_line = !lone.checked();
    settings.tab_width = 8;
    lone.load(settings);
    EXPECT_EQ(told, 0);
    lone.toggle();
    EXPECT_GE(told, 1);
}

TEST_F(SettingsWindowTest, AChangeEnablesApplyAndUpdatesTheWorkingCopy) {
    window_->open();
    page_->toggle();
    EXPECT_TRUE(apply_enabled());
    EXPECT_FALSE(window_->working().highlight_current_line);
    EXPECT_EQ(window_->working().font, std::string("Mono 9"));
    EXPECT_TRUE(applied_.empty());
}

TEST_F(SettingsWindowTest, ChangingBackDisablesApplyAgain) {
    window_->open();
    page_->toggle();
    page_->toggle();
    EXPECT_FALSE(apply_enabled());
    EXPECT_TRUE(window_->working() == file_);
}

TEST_F(SettingsWindowTest, AValueThatNormalisesToTheOldOneIsNoChange) {
    file_.tab_width = 4;
    window_->open();
    page_->type("x");
    EXPECT_EQ(window_->working().tab_width, 4);
    EXPECT_FALSE(apply_enabled());
}

TEST_F(SettingsWindowTest, ApplyGivesTheWorkingCopyToTheHostAndStaysOpen) {
    window_->open();
    page_->toggle();
    click(window_->apply_button());
    ASSERT_EQ(applied_.size(), std::size_t{1});
    EXPECT_FALSE(applied_[0].highlight_current_line);
    EXPECT_EQ(applied_[0].font, std::string("Mono 9"));
    EXPECT_FALSE(apply_enabled());
    EXPECT_TRUE(window_->is_open());
    EXPECT_EQ(closed_, 0);
}

TEST_F(SettingsWindowTest, ApplyWithNothingChangedCallsNothing) {
    window_->open();
    window_->apply();
    EXPECT_TRUE(applied_.empty());
}

TEST_F(SettingsWindowTest, ACopyAppliedOnceIsNotAppliedAgainByOk) {
    window_->open();
    page_->toggle();
    window_->apply();
    window_->ok();
    EXPECT_EQ(applied_.size(), std::size_t{1});
    EXPECT_FALSE(window_->is_open());
}

TEST_F(SettingsWindowTest, AChangeAfterApplyIsComparedWithWhatWasApplied) {
    window_->open();
    page_->toggle();
    window_->apply();
    page_->toggle();
    EXPECT_TRUE(apply_enabled());
    page_->toggle();
    EXPECT_FALSE(apply_enabled());
}

TEST_F(SettingsWindowTest, OkAppliesAndCloses) {
    window_->open();
    page_->toggle();
    click(window_->ok_button());
    ASSERT_EQ(applied_.size(), std::size_t{1});
    EXPECT_FALSE(window_->is_open());
    EXPECT_EQ(closed_, 1);
}

TEST_F(SettingsWindowTest, OkWithNothingChangedOnlyCloses) {
    window_->open();
    click(window_->ok_button());
    EXPECT_TRUE(applied_.empty());
    EXPECT_FALSE(window_->is_open());
    EXPECT_EQ(closed_, 1);
}

TEST_F(SettingsWindowTest, CancelThrowsTheChangesAwayAndCloses) {
    window_->open();
    page_->toggle();
    click(window_->cancel_button());
    EXPECT_TRUE(applied_.empty());
    EXPECT_FALSE(window_->is_open());
    EXPECT_EQ(closed_, 1);
}

TEST_F(SettingsWindowTest, TheCloseButtonOfTheTitleBarCancels) {
    window_->open();
    page_->toggle();
    gboolean handled = FALSE;
    g_signal_emit_by_name(window_->window(), "close-request", &handled);
    EXPECT_TRUE(handled);
    EXPECT_TRUE(applied_.empty());
    EXPECT_FALSE(window_->is_open());
    EXPECT_EQ(closed_, 1);
}

TEST_F(SettingsWindowTest, OpeningAgainReadsTheFileAgain) {
    window_->open();
    page_->toggle();
    window_->cancel();
    file_.highlight_current_line = false;
    file_.font = "Other 12";
    window_->open();
    EXPECT_EQ(reads_, 2);
    EXPECT_FALSE(page_->checked());
    EXPECT_FALSE(apply_enabled());
    EXPECT_EQ(window_->working().font, std::string("Other 12"));
}

TEST_F(SettingsWindowTest, OpeningAWindowThatIsOpenKeepsTheChanges) {
    window_->open();
    page_->toggle();
    window_->open();
    EXPECT_EQ(reads_, 1);
    EXPECT_FALSE(page_->checked());
    EXPECT_TRUE(apply_enabled());
}

TEST_F(SettingsWindowTest, CancelOnAHiddenWindowCallsNothing) {
    window_->cancel();
    EXPECT_EQ(closed_, 0);
}

} // namespace
