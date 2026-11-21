// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of Style on a real display: the palettes, the prefer-dark flag, the stylesheet parsing.

#include <gio/gio.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/style.h"
#include "vermilion_resources.h"

namespace {

namespace gtkx = vermilion::gtkx;
using vermilion::ui::Style;

constexpr char kPrefix[] = "/io/github/adamjeremy_sk/Vermilion/";
constexpr char kPreferDark[] = "gtk-application-prefer-dark-theme";

bool g_display = false;

// Every test needs GTK on a real display; without one (a build server) they are skipped.
class StyleTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        g_display = gtk_init_check();
        vermilion_register_resource();
    }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        set_prefer_dark(false);
    }

    static bool prefer_dark() {
        gboolean value = FALSE;
        g_object_get(gtk_settings_get_default(), kPreferDark, &value, nullptr);
        return value != FALSE;
    }

    static void set_prefer_dark(bool dark) {
        g_object_set(gtk_settings_get_default(), kPreferDark, dark ? TRUE : FALSE, nullptr);
    }

    // The text colour that the stylesheet gives the main window (class vm-main): vm_text.
    static GdkRGBA main_text_colour() {
        GtkWidget* window = gtk_window_new(); // owned by GTK until destroyed
        gtk_widget_add_css_class(window, "vm-main");
        GdkRGBA colour = {0, 0, 0, 0};
        gtk_widget_get_color(window, &colour);
        gtk_window_destroy(GTK_WINDOW(window));
        return colour;
    }
};

// The text of the resource below the program's prefix, "" when it is missing.
std::string resource_text(const std::string& name) {
    const std::string path = std::string(kPrefix) + name;
    std::unique_ptr<GBytes, decltype(&g_bytes_unref)> bytes(
        g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
        &g_bytes_unref);
    if (!bytes) {
        return "";
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    return std::string(data, size);
}

// How many parsing errors GTK reports for the stylesheet text.
int parsing_errors(const std::string& css) {
    int count = 0;
    auto provider = gtkx::ObjectPtr<GtkCssProvider>::adopt(gtk_css_provider_new());
    gtkx::SignalConnection errors = gtkx::connect<void(GtkCssProvider*, GtkCssSection*, GError*)>(
        provider.get(), "parsing-error",
        [&count](GtkCssProvider*, GtkCssSection*, GError*) { ++count; });
    gtk_css_provider_load_from_string(provider.get(), css.c_str());
    return count;
}

TEST_F(StyleTest, TheLightPaletteGivesBlackText) {
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    EXPECT_FALSE(style->dark());
    EXPECT_LT(main_text_colour().red, 0.1f);
}

TEST_F(StyleTest, TheDarkPaletteGivesLightText) {
    std::unique_ptr<Style> style = Style::install(true);
    ASSERT_NE(style, nullptr);
    EXPECT_TRUE(style->dark());
    EXPECT_GT(main_text_colour().red, 0.8f);
}

TEST_F(StyleTest, SetDarkSwitchesTheLookBothWays) {
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    EXPECT_TRUE(style->set_dark(true));
    EXPECT_TRUE(style->dark());
    EXPECT_GT(main_text_colour().red, 0.8f);
    EXPECT_TRUE(style->set_dark(false));
    EXPECT_FALSE(style->dark());
    EXPECT_LT(main_text_colour().red, 0.1f);
}

TEST_F(StyleTest, TheSameStateChangesNothing) {
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    EXPECT_TRUE(style->set_dark(false));
    EXPECT_FALSE(style->dark());
    EXPECT_LT(main_text_colour().red, 0.1f);
}

TEST_F(StyleTest, TheFlagFollowsTheMode) {
    std::unique_ptr<Style> style = Style::install(true);
    ASSERT_NE(style, nullptr);
    EXPECT_TRUE(prefer_dark());
    EXPECT_TRUE(style->set_dark(false));
    EXPECT_FALSE(prefer_dark());
    EXPECT_TRUE(style->set_dark(true));
    EXPECT_TRUE(prefer_dark());
}

TEST_F(StyleTest, TheFlagIsOnlySetWhenItDiffers) {
    int announced = 0;
    gtkx::SignalConnection watch = gtkx::connect<void(GObject*, GParamSpec*)>(
        gtk_settings_get_default(), "notify::gtk-application-prefer-dark-theme",
        [&announced](GObject*, GParamSpec*) { ++announced; });
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(announced, 0);
    EXPECT_TRUE(style->set_dark(false));
    EXPECT_EQ(announced, 0);
    EXPECT_TRUE(style->set_dark(true));
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(style->set_dark(true));
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(style->set_dark(false));
    EXPECT_EQ(announced, 2);
}

TEST_F(StyleTest, TheFlagBeforeTheInstallIsRemembered) {
    set_prefer_dark(true);
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    EXPECT_TRUE(style->prefer_dark_at_start());
    EXPECT_FALSE(prefer_dark());
    style.reset();
    set_prefer_dark(false);
    style = Style::install(true);
    ASSERT_NE(style, nullptr);
    EXPECT_FALSE(style->prefer_dark_at_start());
    EXPECT_TRUE(prefer_dark());
}

TEST_F(StyleTest, TheLightSheetsParseWithoutErrors) {
    const std::string css =
        resource_text("classic_light.css") + "\n" + resource_text("classic_structure.css");
    EXPECT_GT(css.size(), 1000u);
    EXPECT_EQ(parsing_errors(css), 0);
}

TEST_F(StyleTest, TheDarkSheetsParseWithoutErrors) {
    const std::string css =
        resource_text("classic_dark.css") + "\n" + resource_text("classic_structure.css");
    EXPECT_GT(css.size(), 1000u);
    EXPECT_EQ(parsing_errors(css), 0);
}

TEST_F(StyleTest, ABrokenSheetIsCounted) {
    EXPECT_GE(parsing_errors("window { color: notacolour; }"), 1);
}

} // namespace
