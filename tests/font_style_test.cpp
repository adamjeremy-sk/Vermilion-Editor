// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of FontStyle on a real display: the rule reaches a widget with the
// class vm-editor.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <memory>
#include <string>

#include "ui/font_css.h"
#include "ui/font_style.h"
#include "ui/gtkx/object_ptr.h"

namespace {

namespace gtkx = vermilion::gtkx;
using vermilion::ui::FontStyle;
using vermilion::ui::kEditorFontClass;

constexpr char kFace[] = "Vermilion Test Face";

bool g_display = false;

// Every test needs GTK on a real display; without one (a build server) they are
// skipped.
class FontStyleTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
    }

    // The font that the stylesheet gives a new window with or without the class
    // vm-editor. A new window each time: a widget keeps the font it first worked
    // out.
    static PangoFontDescription* window_font(bool with_class) {
        GtkWidget* window = gtk_window_new(); // owned by GTK until destroyed
        if (with_class) {
            gtk_widget_add_css_class(window, kEditorFontClass);
        }
        PangoContext* context = gtk_widget_get_pango_context(window); // borrowed
        PangoFontDescription* copy =
            pango_font_description_copy(pango_context_get_font_description(context));
        gtk_window_destroy(GTK_WINDOW(window));
        return copy;
    }

    static std::string family(bool with_class) {
        PangoFontDescription* font = window_font(with_class);
        const char* name = pango_font_description_get_family(font);
        std::string result = name != nullptr ? name : "";
        pango_font_description_free(font);
        return result;
    }

    static int weight(bool with_class) {
        PangoFontDescription* font = window_font(with_class);
        int result = static_cast<int>(pango_font_description_get_weight(font));
        pango_font_description_free(font);
        return result;
    }
};

TEST_F(FontStyleTest, TheFontIsEmptyAtFirst) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    EXPECT_EQ(style->font(), std::string(""));
}

TEST_F(FontStyleTest, TheFontIsKeptAsGiven) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font("  Mono 11 ");
    EXPECT_EQ(style->font(), std::string("  Mono 11 "));
    style->set_font("");
    EXPECT_EQ(style->font(), std::string(""));
}

TEST_F(FontStyleTest, TheFamilyReachesAWidgetWithTheClass) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " 12");
    EXPECT_EQ(family(true), std::string(kFace));
}

TEST_F(FontStyleTest, AWidgetWithoutTheClassKeepsItsFont) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " 12");
    EXPECT_NE(family(false), std::string(kFace));
}

TEST_F(FontStyleTest, TheWeightReachesAWidgetWithTheClass) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " Bold 12");
    EXPECT_EQ(weight(true), static_cast<int>(PANGO_WEIGHT_BOLD));
    EXPECT_NE(weight(false), static_cast<int>(PANGO_WEIGHT_BOLD));
}

TEST_F(FontStyleTest, TheLastFontWins) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font("Vermilion First Face 12");
    style->set_font("Vermilion Second Face 12");
    EXPECT_EQ(family(true), std::string("Vermilion Second Face"));
}

TEST_F(FontStyleTest, AnEmptyFontRemovesTheRule) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " 12");
    ASSERT_EQ(family(true), std::string(kFace));
    style->set_font("");
    EXPECT_NE(family(true), std::string(kFace));
}

TEST_F(FontStyleTest, DestroyingTheStyleRemovesTheRule) {
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " 12");
    ASSERT_EQ(family(true), std::string(kFace));
    style.reset();
    EXPECT_NE(family(true), std::string(kFace));
}

TEST_F(FontStyleTest, ItBeatsAnEarlierRuleOfTheSamePriority) {
    auto earlier = gtkx::ObjectPtr<GtkCssProvider>::adopt(gtk_css_provider_new());
    gtk_css_provider_load_from_string(earlier.get(),
                                      ".vm-editor { font-family: \"Vermilion Earlier Face\"; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(earlier.get()),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    std::unique_ptr<FontStyle> style = FontStyle::install();
    ASSERT_NE(style, nullptr);
    style->set_font(std::string(kFace) + " 12");
    const std::string seen = family(true);
    gtk_style_context_remove_provider_for_display(gdk_display_get_default(),
                                                  GTK_STYLE_PROVIDER(earlier.get()));
    EXPECT_EQ(seen, std::string(kFace));
}

} // namespace
