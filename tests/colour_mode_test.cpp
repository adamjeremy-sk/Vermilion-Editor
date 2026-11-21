// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for colour_mode: the text form of the mode and the decision between light and dark.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "core/colour_mode.h"

namespace {

using vermilion::colour_mode_id;
using vermilion::ColourMode;
using vermilion::parse_colour_mode;
using vermilion::portal_dark_from_color_scheme;
using vermilion::resolve_dark;
using vermilion::SystemHints;

SystemHints hints(std::optional<bool> portal, const std::string& theme, bool prefer) {
    SystemHints result;
    result.portal_dark = portal;
    result.theme_name = theme;
    result.prefer_dark_at_start = prefer;
    return result;
}

TEST(ColourModeText, TheIdsAreTheStoredWords) {
    EXPECT_EQ(std::string(colour_mode_id(ColourMode::Light)), std::string("light"));
    EXPECT_EQ(std::string(colour_mode_id(ColourMode::Dark)), std::string("dark"));
    EXPECT_EQ(std::string(colour_mode_id(ColourMode::System)), std::string("system"));
}

TEST(ColourModeText, TheKnownWordsParse) {
    EXPECT_EQ(parse_colour_mode("light"), ColourMode::Light);
    EXPECT_EQ(parse_colour_mode("dark"), ColourMode::Dark);
    EXPECT_EQ(parse_colour_mode("system"), ColourMode::System);
}

TEST(ColourModeText, EveryIdParsesBack) {
    for (ColourMode mode : {ColourMode::Light, ColourMode::Dark, ColourMode::System})
        EXPECT_EQ(parse_colour_mode(colour_mode_id(mode)), mode);
}

TEST(ColourModeText, CaseAndBlanksAreIgnored) {
    EXPECT_EQ(parse_colour_mode("Dark"), ColourMode::Dark);
    EXPECT_EQ(parse_colour_mode("LIGHT"), ColourMode::Light);
    EXPECT_EQ(parse_colour_mode("  dark\t"), ColourMode::Dark);
    EXPECT_EQ(parse_colour_mode("\tLight "), ColourMode::Light);
}

TEST(ColourModeText, AnythingElseIsSystem) {
    EXPECT_EQ(parse_colour_mode(""), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("   "), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("auto"), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("darkish"), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("dark mode"), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("d"), ColourMode::System);
    EXPECT_EQ(parse_colour_mode("1"), ColourMode::System);
}

TEST(ResolveDark, LightAndDarkIgnoreTheHints) {
    const SystemHints dark_hints = hints(true, "Greybird-dark", true);
    const SystemHints light_hints = hints(false, "Greybird", false);
    EXPECT_FALSE(resolve_dark(ColourMode::Light, dark_hints));
    EXPECT_FALSE(resolve_dark(ColourMode::Light, light_hints));
    EXPECT_TRUE(resolve_dark(ColourMode::Dark, dark_hints));
    EXPECT_TRUE(resolve_dark(ColourMode::Dark, light_hints));
}

TEST(ResolveDark, ThePortalAnswerComesFirst) {
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(true, "Greybird", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(false, "Greybird-dark", true)));
}

TEST(ResolveDark, WithoutAPortalAThemeNameEndingInDarkIsDark) {
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "Greybird-dark", false)));
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "Adwaita-dark", false)));
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "Arc-Dark", false)));
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "Yaru-DARK", false)));
}

TEST(ResolveDark, OtherThemeNamesAreNotDark) {
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "Greybird", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "Adwaita", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "dark", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "-dark", false)));
    EXPECT_FALSE(
        resolve_dark(ColourMode::System, hints(std::nullopt, "Greybird-dark-compact", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "Midnight-darkness", false)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "", false)));
}

TEST(ResolveDark, TheFlagAtStartIsTheLastResort) {
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "Adwaita", true)));
    EXPECT_TRUE(resolve_dark(ColourMode::System, hints(std::nullopt, "", true)));
    EXPECT_FALSE(resolve_dark(ColourMode::System, hints(std::nullopt, "Adwaita", false)));
}

TEST(ResolveDark, NoHintsAtAllIsLight) {
    EXPECT_FALSE(resolve_dark(ColourMode::System, SystemHints{}));
}

TEST(PortalColourScheme, OneIsDarkAndTwoIsLight) {
    EXPECT_EQ(portal_dark_from_color_scheme(1), std::optional<bool>(true));
    EXPECT_EQ(portal_dark_from_color_scheme(2), std::optional<bool>(false));
}

TEST(PortalColourScheme, ZeroAndUnknownValuesGiveNoAnswer) {
    EXPECT_EQ(portal_dark_from_color_scheme(0), std::nullopt);
    EXPECT_EQ(portal_dark_from_color_scheme(3), std::nullopt);
    EXPECT_EQ(portal_dark_from_color_scheme(UINT32_MAX), std::nullopt);
}

} // namespace
