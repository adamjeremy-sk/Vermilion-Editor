// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the bar and the keys of HelpWindow on a real display (the tests skip without one): Hide
// and Show, Back and Forward, Ctrl+F, F3 and Shift+F3, and the pane at the start.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/search_messages.h"
#include "core/search_options.h"
#include "ui/help_contents.h"
#include "ui/help_page_view.h"
#include "ui/help_search.h"
#include "ui/help_window.h"
#include "ui/manual_library.h"
#include "ui/window_shortcuts.h"
#include "vermilion_manual.h"
#include "vermilion_resources.h"

namespace {

using vermilion::ui::HelpWindow;
using vermilion::ui::ManualLibrary;

bool g_display = false;

// Runs the main loop for about `ms` milliseconds.
void spin(int ms) {
    const gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

// Runs the main loop until `done()` is true, for at most two seconds; returns `done()`.
template <typename Done>
bool spin_until(Done done) {
    const gint64 end = g_get_monotonic_time() + 2 * G_USEC_PER_SEC;
    while (!done() && g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
    return done();
}

// The keys that `app` has for `action`, as GTK spells them.
std::vector<std::string> keys_of(GtkApplication* app, const char* action) {
    std::unique_ptr<char*, void (*)(char**)> accels(
        gtk_application_get_accels_for_action(app, action), &g_strfreev);
    std::vector<std::string> keys;
    for (char** key = accels.get(); key != nullptr && *key != nullptr; ++key) {
        keys.emplace_back(*key);
    }
    return keys;
}

// The search that the tests run: a word that many pages of the manual have.
vermilion::SearchOptions settings_search() {
    vermilion::SearchOptions options;
    options.text = "settings";
    return options;
}

class HelpWindowBarTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        vermilion_manual_register_resource();
        vermilion_register_resource();
        g_display = gtk_init_check();
    }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        app_.reset(gtk_application_new("io.github.adamjeremy_sk.VermilionHelpWindowBarTest",
                                       G_APPLICATION_NON_UNIQUE));
        ASSERT_TRUE(g_application_register(G_APPLICATION(app_.get()), nullptr, nullptr));
        vermilion::ui::set_shortcuts(app_.get()); // what the main window does at the start
        parent_ = GTK_WINDOW(gtk_window_new());   // toplevel; destroyed in TearDown
        library_.prepare();
        const gint64 end = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
        while (!library_.ready() && g_get_monotonic_time() < end) {
            g_main_context_iteration(nullptr, FALSE);
            g_usleep(1000);
        }
        ASSERT_TRUE(library_.ready());
        help_ = std::make_unique<HelpWindow>(app_.get(), parent_, library_);
    }

    void TearDown() override {
        if (help_ != nullptr) {
            GtkWindow* window = help_->window();
            help_.reset();
            gtk_window_destroy(window);
        }
        if (parent_ != nullptr) {
            gtk_window_destroy(parent_);
        }
    }

    // The bar of buttons: the first row of the window.
    GtkWidget* bar() { return gtk_widget_get_first_child(gtk_window_get_child(help_->window())); }
    // The status line: the last row of the window.
    GtkWidget* status_line() {
        return gtk_widget_get_last_child(gtk_window_get_child(help_->window()));
    }
    // The buttons of the bar, left to right.
    std::vector<GtkWidget*> buttons() {
        std::vector<GtkWidget*> found;
        for (GtkWidget* child = gtk_widget_get_first_child(bar()); child != nullptr;
             child = gtk_widget_get_next_sibling(child)) {
            found.push_back(child);
        }
        return found;
    }
    // The button of the bar that says `label`, or null.
    GtkWidget* button(const std::string& label) {
        for (GtkWidget* child : buttons()) {
            if (label_of(child) == label) {
                return child;
            }
        }
        return nullptr;
    }
    static std::string label_of(GtkWidget* button) {
        return gtk_button_get_label(GTK_BUTTON(button));
    }
    // A click on `button`.
    static void click(GtkWidget* button) { g_signal_emit_by_name(button, "clicked"); }
    // What a key of the action does.
    void activate(const char* action) {
        ASSERT_TRUE(gtk_widget_activate_action(GTK_WIDGET(help_->window()), action, nullptr));
    }
    // True when the window's action `name` ("back", "forward") can be used.
    bool enabled(const char* name) {
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(help_->window()), name);
        return action != nullptr && g_action_get_enabled(action) != FALSE;
    }
    // The side pane: the notebook that holds the tree and the Search tab.
    GtkWidget* pane() {
        return gtk_widget_get_ancestor(help_->contents().widget(), GTK_TYPE_NOTEBOOK);
    }
    const std::string& text() { return help_->page_view().page().text; }

    std::unique_ptr<GtkApplication, void (*)(gpointer)> app_{nullptr, &g_object_unref};
    GtkWindow* parent_ = nullptr;
    ManualLibrary library_; // before help_, so it is destroyed after it
    std::unique_ptr<HelpWindow> help_;
};

TEST_F(HelpWindowBarTest, TheButtonsOfTheBarAreInOrder) {
    const std::vector<GtkWidget*> bar = buttons();
    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Hide", "win.toggle-pane"},
        {"Back", "win.back"},
        {"Forward", "win.forward"},
    };
    ASSERT_EQ(bar.size(), expected.size());
    for (std::size_t i = 0; i < bar.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(label_of(bar[i]), expected[i].first);
        EXPECT_STREQ(gtk_actionable_get_action_name(GTK_ACTIONABLE(bar[i])),
                     expected[i].second.c_str());
    }
}

TEST_F(HelpWindowBarTest, EveryWindowOpensOnTheHomePageWithThePaneOnItsContentsTab) {
    EXPECT_TRUE(help_->pane_shown());
    EXPECT_TRUE(gtk_widget_get_visible(pane()));
    EXPECT_FALSE(help_->search_tab_shown());
    EXPECT_EQ(help_->contents().current(), "index.md");
    EXPECT_FALSE(enabled("back"));
    EXPECT_FALSE(enabled("forward"));
    help_->show_search();
    help_->set_pane_shown(false);
    HelpWindow second(app_.get(), parent_, library_);
    EXPECT_TRUE(second.pane_shown());
    EXPECT_FALSE(second.search_tab_shown());
    EXPECT_EQ(second.contents().current(), "index.md");
    gtk_window_destroy(second.window());
}

TEST_F(HelpWindowBarTest, ForwardGoesToThePageThatBackLeft) {
    const std::string index = text();
    help_->open_topic("settings.md");
    const std::string settings = text();
    help_->open_topic("help.md");
    const std::string help = text();
    EXPECT_FALSE(enabled("forward"));
    help_->back();
    help_->back();
    EXPECT_EQ(text(), index);
    EXPECT_FALSE(enabled("back"));
    EXPECT_TRUE(enabled("forward"));
    help_->forward();
    EXPECT_EQ(text(), settings);
    EXPECT_EQ(help_->contents().current(), "settings.md");
    EXPECT_TRUE(enabled("back"));
    EXPECT_TRUE(enabled("forward"));
    help_->forward();
    EXPECT_EQ(text(), help);
    EXPECT_FALSE(enabled("forward"));
    help_->forward(); // nothing to go to
    EXPECT_EQ(text(), help);
    help_->back();
    help_->open_topic("index.md"); // a page opened by hand empties the forward list
    EXPECT_FALSE(enabled("forward"));
}

TEST_F(HelpWindowBarTest, TheBackAndForwardButtonsAndKeysGoThroughThePages) {
    EXPECT_EQ(keys_of(app_.get(), "win.back"), std::vector<std::string>{"<Alt>Left"});
    EXPECT_EQ(keys_of(app_.get(), "win.forward"), std::vector<std::string>{"<Alt>Right"});
    help_->open_topic("settings.md");
    click(button("Back"));
    EXPECT_EQ(help_->contents().current(), "index.md");
    click(button("Forward"));
    EXPECT_EQ(help_->contents().current(), "settings.md");
    activate("win.back");
    EXPECT_EQ(help_->contents().current(), "index.md");
    activate("win.forward");
    EXPECT_EQ(help_->contents().current(), "settings.md");
}

TEST_F(HelpWindowBarTest, HideGivesThePanesRoomToThePageAndShowGivesItBack) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    gtk_window_present(help_->window());
    spin(500);
    GtkWidget* hide = button("Hide");
    ASSERT_NE(hide, nullptr);
    GtkWidget* split = gtk_widget_get_parent(pane());
    const int window_width = gtk_widget_get_width(GTK_WIDGET(help_->window()));
    const int page_width = gtk_widget_get_width(view.widget());
    const int position = gtk_paned_get_position(GTK_PANED(split));
    const std::size_t columns = view.columns();
    click(hide);
    EXPECT_FALSE(help_->pane_shown());
    EXPECT_FALSE(gtk_widget_get_visible(pane()));
    EXPECT_EQ(label_of(hide), "Show");
    EXPECT_STREQ(gtk_widget_get_tooltip_text(hide), "Show the side pane");
    EXPECT_TRUE(spin_until([&] { return view.columns() > columns; }));
    EXPECT_EQ(gtk_widget_get_width(view.widget()), gtk_widget_get_width(split));
    EXPECT_GT(gtk_widget_get_width(view.widget()), page_width);
    EXPECT_EQ(gtk_widget_get_width(GTK_WIDGET(help_->window())), window_width);
    EXPECT_TRUE(gtk_widget_get_mapped(status_line()));
    click(hide);
    EXPECT_TRUE(help_->pane_shown());
    EXPECT_EQ(label_of(hide), "Hide");
    EXPECT_STREQ(gtk_widget_get_tooltip_text(hide), "Hide the side pane");
    EXPECT_TRUE(spin_until([&] { return view.columns() == columns; }));
    EXPECT_EQ(gtk_paned_get_position(GTK_PANED(split)), position);
    EXPECT_EQ(gtk_widget_get_width(view.widget()), page_width);
}

TEST_F(HelpWindowBarTest, CtrlFShowsTheSearchTabWithTheKeyboardInTheBox) {
    EXPECT_EQ(keys_of(app_.get(), "win.find"), std::vector<std::string>{"<Control>f"});
    gtk_window_present(help_->window());
    spin(300);
    activate("win.find");
    EXPECT_TRUE(help_->search_tab_shown());
    GtkWidget* focus = gtk_window_get_focus(help_->window()); // borrowed
    ASSERT_NE(focus, nullptr);
    EXPECT_TRUE(gtk_widget_is_ancestor(focus, help_->search().widget()));
}

TEST_F(HelpWindowBarTest, CtrlFShowsAHiddenPaneAgain) {
    gtk_window_present(help_->window());
    spin(300);
    help_->set_pane_shown(false);
    activate("win.find");
    EXPECT_TRUE(help_->pane_shown());
    EXPECT_EQ(label_of(buttons()[0]), "Hide");
    EXPECT_TRUE(help_->search_tab_shown());
    GtkWidget* focus = gtk_window_get_focus(help_->window()); // borrowed
    ASSERT_NE(focus, nullptr);
    EXPECT_TRUE(gtk_widget_is_ancestor(focus, help_->search().widget()));
}

TEST_F(HelpWindowBarTest, F3AndShiftF3StepThroughTheMatchesOfThePage) {
    EXPECT_EQ(keys_of(app_.get(), "win.find-next"), std::vector<std::string>{"F3"});
    EXPECT_EQ(keys_of(app_.get(), "win.find-previous"), std::vector<std::string>{"<Shift>F3"});
    help_->run_search(settings_search());
    help_->open_topic("settings.md");
    const int count = static_cast<int>(help_->page_view().matches().size());
    ASSERT_GE(count, 3);
    activate("win.find-next");
    EXPECT_EQ(help_->page_view().current_match(), std::optional<std::size_t>(0));
    EXPECT_EQ(help_->status_text(), vermilion::match_count_message(1, count));
    activate("win.find-next");
    EXPECT_EQ(help_->page_view().current_match(), std::optional<std::size_t>(1));
    activate("win.find-previous");
    EXPECT_EQ(help_->page_view().current_match(), std::optional<std::size_t>(0));
    help_->set_pane_shown(false); // the keys work while the pane is hidden
    activate("win.find-previous");
    EXPECT_EQ(help_->page_view().current_match(),
              std::optional<std::size_t>(static_cast<std::size_t>(count) - 1));
    EXPECT_EQ(help_->status_text(), vermilion::wrapped_message(false));
}

} // namespace
