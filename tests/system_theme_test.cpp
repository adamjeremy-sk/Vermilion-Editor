// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of SystemTheme: the portal's value, the theme name that GTK reports, and the calls made
// when either changes. They need GTK on a real display and are skipped without one; none of them
// talks to D-Bus.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/colour_mode.h"
#include "ui/system_theme.h"

namespace {

using vermilion::SystemHints;
using vermilion::ui::SystemTheme;

bool g_display = false;

// A GVariant of the portal's colour scheme: the value `scheme`, wrapped in `wraps` variants.
GVariant* scheme_variant(std::uint32_t scheme, int wraps) {
    GVariant* value = g_variant_new_uint32(scheme);
    for (int i = 0; i < wraps; ++i) {
        value = g_variant_new_variant(value);
    }
    return g_variant_ref_sink(value);
}

class PortalValueTest : public ::testing::Test {};

TEST_F(PortalValueTest, TheValueItselfIsRead) {
    GVariant* dark = scheme_variant(1, 0);
    GVariant* light = scheme_variant(2, 0);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(dark), std::optional<bool>(true));
    EXPECT_EQ(SystemTheme::dark_from_portal_value(light), std::optional<bool>(false));
    g_variant_unref(dark);
    g_variant_unref(light);
}

TEST_F(PortalValueTest, WrappedValuesAreUnwrapped) {
    for (int wraps = 1; wraps <= 3; ++wraps) {
        GVariant* dark = scheme_variant(1, wraps);
        GVariant* light = scheme_variant(2, wraps);
        EXPECT_EQ(SystemTheme::dark_from_portal_value(dark), std::optional<bool>(true));
        EXPECT_EQ(SystemTheme::dark_from_portal_value(light), std::optional<bool>(false));
        g_variant_unref(dark);
        g_variant_unref(light);
    }
}

TEST_F(PortalValueTest, NoPreferenceAndUnknownValuesGiveNothing) {
    GVariant* none = scheme_variant(0, 2);
    GVariant* other = scheme_variant(7, 2);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(none), std::nullopt);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(other), std::nullopt);
    g_variant_unref(none);
    g_variant_unref(other);
}

TEST_F(PortalValueTest, ValuesOfOtherTypesAndNullGiveNothing) {
    GVariant* text = g_variant_ref_sink(g_variant_new_string("dark"));
    GVariant* wrapped_text = g_variant_ref_sink(g_variant_new_variant(g_variant_new_string("x")));
    GVariant* signed_one = g_variant_ref_sink(g_variant_new_int32(1));
    EXPECT_EQ(SystemTheme::dark_from_portal_value(text), std::nullopt);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(wrapped_text), std::nullopt);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(signed_one), std::nullopt);
    EXPECT_EQ(SystemTheme::dark_from_portal_value(nullptr), std::nullopt);
    g_variant_unref(text);
    g_variant_unref(wrapped_text);
    g_variant_unref(signed_one);
}

// The tests that use the settings of GTK: the theme name is put back after each one.
class SystemThemeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        settings_ = gtk_settings_get_default(); // borrowed
        gchar* raw = nullptr;
        g_object_get(settings_, "gtk-theme-name", &raw, nullptr);
        std::unique_ptr<gchar, decltype(&g_free)> owned(raw, &g_free);
        original_ = owned ? owned.get() : "";
        set_theme("Plain");
    }

    void TearDown() override {
        if (g_display) {
            set_theme(original_);
        }
    }

    void set_theme(const std::string& name) {
        g_object_set(settings_, "gtk-theme-name", name.c_str(), nullptr);
    }

    std::unique_ptr<SystemTheme> make(bool prefer_dark_at_start = false) {
        return SystemTheme::create(prefer_dark_at_start, false,
                                   [this](const SystemHints& hints) { calls_.push_back(hints); });
    }

    GtkSettings* settings_ = nullptr;
    std::string original_;
    std::vector<SystemHints> calls_;
};

TEST_F(SystemThemeTest, TheHintsAtTheStartAreTheThemeAndTheFlag) {
    std::unique_ptr<SystemTheme> theme = make(true);
    ASSERT_NE(theme, nullptr);
    EXPECT_EQ(theme->hints().theme_name, std::string("Plain"));
    EXPECT_TRUE(theme->hints().prefer_dark_at_start);
    EXPECT_EQ(theme->hints().portal_dark, std::nullopt);
    EXPECT_TRUE(calls_.empty());
}

TEST_F(SystemThemeTest, AChangeOfTheThemeNameIsReportedOnce) {
    std::unique_ptr<SystemTheme> theme = make();
    ASSERT_NE(theme, nullptr);
    set_theme("Plain-dark");
    ASSERT_EQ(calls_.size(), std::size_t{1});
    EXPECT_EQ(calls_[0].theme_name, std::string("Plain-dark"));
    EXPECT_EQ(theme->hints().theme_name, std::string("Plain-dark"));
    set_theme("Plain-dark");
    EXPECT_EQ(calls_.size(), std::size_t{1});
    set_theme("Plain");
    ASSERT_EQ(calls_.size(), std::size_t{2});
    EXPECT_EQ(calls_[1].theme_name, std::string("Plain"));
}

TEST_F(SystemThemeTest, APortalValueIsReportedWhenItChanges) {
    std::unique_ptr<SystemTheme> theme = make(true);
    ASSERT_NE(theme, nullptr);
    GVariant* dark = scheme_variant(1, 2);
    GVariant* light = scheme_variant(2, 2);
    GVariant* none = scheme_variant(0, 2);
    theme->set_portal_value(dark);
    ASSERT_EQ(calls_.size(), std::size_t{1});
    EXPECT_EQ(calls_[0].portal_dark, std::optional<bool>(true));
    EXPECT_EQ(calls_[0].theme_name, std::string("Plain"));
    EXPECT_TRUE(calls_[0].prefer_dark_at_start);
    theme->set_portal_value(dark);
    EXPECT_EQ(calls_.size(), std::size_t{1});
    theme->set_portal_value(light);
    ASSERT_EQ(calls_.size(), std::size_t{2});
    EXPECT_EQ(calls_[1].portal_dark, std::optional<bool>(false));
    theme->set_portal_value(none);
    ASSERT_EQ(calls_.size(), std::size_t{3});
    EXPECT_EQ(calls_[2].portal_dark, std::nullopt);
    theme->set_portal_value(nullptr);
    EXPECT_EQ(calls_.size(), std::size_t{3});
    g_variant_unref(dark);
    g_variant_unref(light);
    g_variant_unref(none);
}

TEST_F(SystemThemeTest, NothingIsCalledAfterTheThemeIsGone) {
    std::unique_ptr<SystemTheme> theme = make();
    ASSERT_NE(theme, nullptr);
    theme.reset();
    set_theme("Plain-dark");
    EXPECT_TRUE(calls_.empty());
}

} // namespace
