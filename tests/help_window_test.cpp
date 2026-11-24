// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpWindow on a real display (the tests skip without one): the page view in the window,
// links, Back and Index, the size at which it opens, and the colours after a palette change.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_spans.h"
#include "core/manual_text.h"
#include "ui/help_page_view.h"
#include "ui/help_window.h"
#include "ui/style.h"
#include "vermilion_manual.h"
#include "vermilion_resources.h"

namespace {

using vermilion::ManualText;
using vermilion::ui::HelpWindow;

bool g_display = false;

// The text that the page view shows for a topic file of the manual folder.
ManualText page_of_file(const std::string& name) {
    std::ifstream file(std::string(VERMILION_MANUAL_DIR) + "/" + name);
    std::stringstream text;
    text << file.rdbuf();
    return vermilion::build_manual_text(vermilion::parse_manual(text.str()),
                                        vermilion::kManualColumns);
}

// Runs the main loop for about `ms` milliseconds.
void spin(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

// A palette with every colour name the Help page tags read (the link first); `shift` changes them
// all.
std::string palette_css(int shift) {
    const char* names[] = {"vm_link", "vm_light", "vm_chassis", "vm_field_text"};
    std::string css;
    int n = 0;
    for (const char* name : names) {
        char hex[16];
        g_snprintf(hex, sizeof hex, "#%02x%02x%02x", 16 * (n + 1) + shift, 40 + n, 200 - n);
        css += std::string("@define-color ") + name + " " + hex + ";\n";
        ++n;
    }
    return css;
}

// The foreground colour of the link tag, as text ("" when it is not set).
std::string link_colour(HelpWindow& help) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(help.page_view().text_view());
    GtkTextTag* tag =
        gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), vermilion::ui::kTagLink);
    gboolean set = FALSE;
    GdkRGBA* colour = nullptr;
    g_object_get(tag, "foreground-set", &set, "foreground-rgba", &colour, nullptr);
    std::string text;
    if (set != FALSE && colour != nullptr) {
        char* spelled = gdk_rgba_to_string(colour);
        text = spelled;
        g_free(spelled);
    }
    if (colour != nullptr) {
        gdk_rgba_free(colour);
    }
    return text;
}

// A stylesheet with the palette on the display for as long as the object lives.
class PaletteOnDisplay {
public:
    explicit PaletteOnDisplay(int shift) : provider_(gtk_css_provider_new()) {
        load(shift);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                   GTK_STYLE_PROVIDER(provider_),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    ~PaletteOnDisplay() {
        gtk_style_context_remove_provider_for_display(gdk_display_get_default(),
                                                      GTK_STYLE_PROVIDER(provider_));
        g_object_unref(provider_);
    }
    PaletteOnDisplay(const PaletteOnDisplay&) = delete;
    PaletteOnDisplay& operator=(const PaletteOnDisplay&) = delete;
    // Replaces the palette, as the program's Style does when the colour mode changes.
    void load(int shift) {
        gtk_css_provider_load_from_string(provider_, palette_css(shift).c_str());
    }

private:
    GtkCssProvider* provider_;
};

// The text that link_colour() gives for a colour written as "#rrggbb".
std::string colour_text(const char* hex) {
    GdkRGBA colour;
    EXPECT_TRUE(gdk_rgba_parse(&colour, hex));
    char* spelled = gdk_rgba_to_string(&colour);
    std::string text = spelled;
    g_free(spelled);
    return text;
}

// The link colour that palette_css(shift) gives, spelled as link_colour() spells it.
std::string link_colour_of(int shift) {
    char hex[16];
    g_snprintf(hex, sizeof hex, "#%02x%02x%02x", 16 + shift, 40, 200);
    GdkRGBA colour;
    EXPECT_TRUE(gdk_rgba_parse(&colour, hex));
    char* spelled = gdk_rgba_to_string(&colour);
    std::string text = spelled;
    g_free(spelled);
    return text;
}

// Turns GTK's dark flag round (the program's Style does it after it has loaded a palette) and
// returns the value it had.
bool flip_dark_flag() {
    GtkSettings* settings = gtk_settings_get_default();
    gboolean dark = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &dark, nullptr);
    g_object_set(settings, "gtk-application-prefer-dark-theme", dark == FALSE, nullptr);
    return dark != FALSE;
}

class HelpWindowTest : public ::testing::Test {
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
        app_.reset(gtk_application_new("io.github.adamjeremy_sk.VermilionHelpWindowTest",
                                       G_APPLICATION_NON_UNIQUE));
        ASSERT_TRUE(g_application_register(G_APPLICATION(app_.get()), nullptr, nullptr));
        parent_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        help_ = std::make_unique<HelpWindow>(app_.get(), parent_);
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

    const ManualText& page() { return help_->page_view().page(); }

    // True when the action "back" of the window can be used.
    bool back_enabled() const {
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(help_->window()), "back");
        return action != nullptr && g_action_get_enabled(action) != FALSE;
    }

    std::unique_ptr<GtkApplication, void (*)(gpointer)> app_{nullptr, &g_object_unref};
    GtkWindow* parent_ = nullptr;
    std::unique_ptr<HelpWindow> help_;
};

TEST_F(HelpWindowTest, ItOpensOnTheIndexPage) {
    EXPECT_EQ(page().text, page_of_file("index.md").text);
    EXPECT_FALSE(page().text.empty());
    EXPECT_FALSE(back_enabled());
    EXPECT_EQ(gtk_window_get_transient_for(help_->window()), parent_);
    EXPECT_EQ(std::string(gtk_window_get_title(help_->window())),
              std::string("Vermilion Editor Help"));
    EXPECT_TRUE(gtk_widget_has_css_class(GTK_WIDGET(help_->window()), "vm-help"));
    EXPECT_TRUE(gtk_widget_has_css_class(GTK_WIDGET(help_->window()), "vm-main"));
}

TEST_F(HelpWindowTest, AClickOnALinkOpensItsTopicAndBackComesBack) {
    ASSERT_FALSE(page().links.empty());
    const std::string target = page().links[0].target;
    help_->page_view().activate_at(page().links[0].begin);
    EXPECT_EQ(page().text, page_of_file(target).text);
    EXPECT_NE(page().text, page_of_file("index.md").text);
    EXPECT_TRUE(back_enabled());
    help_->back();
    EXPECT_EQ(page().text, page_of_file("index.md").text);
    EXPECT_FALSE(back_enabled());
}

TEST_F(HelpWindowTest, TheIndexGoesBackToTheIndexPage) {
    ASSERT_FALSE(page().links.empty());
    help_->page_view().activate_at(page().links[0].begin);
    ASSERT_NE(page().text, page_of_file("index.md").text);
    help_->show_index();
    EXPECT_EQ(page().text, page_of_file("index.md").text);
    EXPECT_TRUE(back_enabled());
}

TEST_F(HelpWindowTest, OpeningThePageThatIsOpenChangesNothing) {
    help_->show_index();
    EXPECT_FALSE(back_enabled());
    help_->open_topic("index.md");
    EXPECT_FALSE(back_enabled());
}

TEST_F(HelpWindowTest, EveryTopicOfTheManualOpens) {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(VERMILION_MANUAL_DIR)) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".md") {
            continue;
        }
        SCOPED_TRACE(name);
        help_->open_topic(name);
        EXPECT_EQ(page().text, page_of_file(name).text);
        EXPECT_FALSE(page().text.empty());
        ++count;
    }
    EXPECT_GE(count, 10);
}

TEST_F(HelpWindowTest, ATopicThatIsNotInTheProgramSaysSo) {
    ASSERT_TRUE(vermilion::is_topic_name("no-such-topic.md"));
    help_->open_topic("no-such-topic.md");
    EXPECT_NE(page().text.find("This topic is not available: no-such-topic.md"), std::string::npos);
    EXPECT_TRUE(back_enabled());
}

TEST_F(HelpWindowTest, ANameThatIsNotATopicIsIgnored) {
    const std::string before = page().text;
    for (const char* name : {"", "../index.md", "http://example.org/index.md", "index", "a b.md"}) {
        SCOPED_TRACE(name);
        ASSERT_FALSE(vermilion::is_topic_name(name));
        help_->open_topic(name);
        EXPECT_EQ(page().text, before);
        EXPECT_FALSE(back_enabled());
    }
}

TEST_F(HelpWindowTest, ItOpensThirtySixLinesHighAndAsWideAsThePage) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    gtk_window_present(help_->window());
    spin(500);
    // The text area is exactly 36 lines and the margins above and below.
    EXPECT_EQ(static_cast<int>(gtk_adjustment_get_page_size(view.vadjustment())),
              view.height_for_lines(vermilion::ui::kHelpPageLines));
    EXPECT_EQ(vermilion::ui::kHelpPageLines, 36);
    EXPECT_EQ(gtk_widget_get_width(GTK_WIDGET(help_->window())), view.natural_width());
    EXPECT_EQ(gtk_widget_get_width(view.widget()), view.natural_width());
    EXPECT_GT(view.natural_width(), view.page_width());
    EXPECT_LT(view.natural_width(), view.page_width() + 60);
}

TEST_F(HelpWindowTest, TheNaturalWidthFollowsTheZoom) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    const int before = view.natural_width();
    const int bar = before - view.page_width();
    ASSERT_TRUE(view.set_zoom(2.0));
    EXPECT_EQ(view.natural_width(), view.page_width() + bar);
    EXPECT_GT(view.natural_width(), before * 1.8); // twice the page, whatever the font is
}

TEST_F(HelpWindowTest, ThePageReadsItsColoursAgainWhenTheDarkFlagChanges) {
    PaletteOnDisplay palette(0);
    help_->page_view().refresh_colours();
    EXPECT_EQ(link_colour(*help_), link_colour_of(0));
    palette.load(5);
    EXPECT_EQ(link_colour(*help_), link_colour_of(0)); // nothing has told the page yet
    const bool was = flip_dark_flag();
    EXPECT_EQ(link_colour(*help_), link_colour_of(5));
    palette.load(9);
    flip_dark_flag();
    EXPECT_EQ(link_colour(*help_), link_colour_of(9));
    if (was != FALSE) {
        flip_dark_flag();
    }
}

// The program's own stylesheets: the dark palette gives the links their light blue.
TEST_F(HelpWindowTest, TheProgramsPaletteSwitchChangesTheColoursOfThePage) {
    GtkSettings* settings = gtk_settings_get_default();
    gboolean flag = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &flag, nullptr);
    {
        std::unique_ptr<vermilion::ui::Style> style = vermilion::ui::Style::install(false);
        ASSERT_NE(style, nullptr);
        help_->page_view().refresh_colours();
        EXPECT_EQ(link_colour(*help_), colour_text("#0a246a"));
        ASSERT_TRUE(style->set_dark(true));
        EXPECT_EQ(link_colour(*help_), colour_text("#7fb2ff"));
        ASSERT_TRUE(style->set_dark(false));
        EXPECT_EQ(link_colour(*help_), colour_text("#0a246a"));
    }
    g_object_set(settings, "gtk-application-prefer-dark-theme", flag, nullptr);
}

TEST_F(HelpWindowTest, AfterTheWindowIsGoneTheFlagMeansNothingToIt) {
    GtkWindow* window = help_->window();
    help_.reset();
    gtk_window_destroy(window);
    const bool was = flip_dark_flag();
    flip_dark_flag();
    if (was) {
        flip_dark_flag();
        flip_dark_flag();
    }
    SUCCEED();
}

} // namespace
