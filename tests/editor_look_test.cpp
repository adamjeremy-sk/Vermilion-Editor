// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for EditorLook: its defaults, equality, and how it is made from the settings.

#include <gtest/gtest.h>

#include <string>

#include "core/editor_look.h"
#include "core/editor_settings.h"

namespace {

using vermilion::default_settings;
using vermilion::EditorLook;
using vermilion::EditorSettings;
using vermilion::make_look;

TEST(EditorLookValue, TheDefaultsAreTheSettingsDefaults) {
    const EditorLook look;
    EXPECT_FALSE(look.dark);
    EXPECT_TRUE(look.highlight_current_line);
    EXPECT_EQ(look.tab_width, 4);
    EXPECT_EQ(look.font, std::string("Fantasque Sans Mono 12"));
    EXPECT_TRUE(look == make_look(default_settings(), false));
}

TEST(EditorLookValue, EqualityLooksAtEveryField) {
    const EditorLook base;
    EditorLook other = base;
    EXPECT_TRUE(other == base);
    EXPECT_FALSE(other != base);
    other.dark = true;
    EXPECT_TRUE(other != base);
    other = base;
    other.highlight_current_line = false;
    EXPECT_TRUE(other != base);
    other = base;
    other.tab_width = 8;
    EXPECT_TRUE(other != base);
    other = base;
    other.font = "Mono 10";
    EXPECT_TRUE(other != base);
}

TEST(EditorLookValue, TheLookTakesTheFourFieldsAndTheDecision) {
    EditorSettings values = default_settings();
    values.highlight_current_line = false;
    values.tab_width = 8;
    values.font = "DejaVu Sans Mono 11";
    const EditorLook dark = make_look(values, true);
    EXPECT_TRUE(dark.dark);
    EXPECT_FALSE(dark.highlight_current_line);
    EXPECT_EQ(dark.tab_width, 8);
    EXPECT_EQ(dark.font, std::string("DejaVu Sans Mono 11"));
    EXPECT_FALSE(make_look(values, false).dark);
}

TEST(EditorLookValue, TheValuesAreNormalisedFirst) {
    EditorSettings values = default_settings();
    values.tab_width = 6;
    values.font = "  Mono 10\t";
    const EditorLook look = make_look(values, false);
    EXPECT_EQ(look.tab_width, 4);
    EXPECT_EQ(look.font, std::string("Mono 10"));
}

TEST(EditorLookValue, TheColourModeAndTheFileSettingsAreNotPartOfTheLook) {
    EditorSettings a = default_settings();
    EditorSettings b = a;
    b.colour_mode = vermilion::ColourMode::Dark;
    b.confirm_large_reload = false;
    b.long_line_limit = 20000;
    EXPECT_TRUE(make_look(a, false) == make_look(b, false));
}

} // namespace
