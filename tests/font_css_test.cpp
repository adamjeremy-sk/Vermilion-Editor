// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for font_css: the rule that gives the editor text its font. No display, no main loop.

#include <gtest/gtest.h>

#include <clocale>
#include <string>

#include "ui/font_css.h"

namespace {

using vermilion::ui::font_css;
using vermilion::ui::kEditorFontClass;

TEST(FontCss, TheClassIsVmEditor) {
    EXPECT_EQ(std::string(kEditorFontClass), std::string("vm-editor"));
}

TEST(FontCss, EmptyOrBlankTextGivesNoRule) {
    EXPECT_EQ(font_css(""), std::string(""));
    EXPECT_EQ(font_css("   "), std::string(""));
    EXPECT_EQ(font_css("\t"), std::string(""));
}

TEST(FontCss, FamilyAndSize) {
    EXPECT_EQ(font_css("DejaVu Sans Mono 11"),
              std::string(".vm-editor {\n"
                          "    font-family: \"DejaVu Sans Mono\";\n"
                          "    font-size: 11pt;\n"
                          "}\n"));
}

TEST(FontCss, AFractionalSizeKeepsItsDecimals) {
    EXPECT_EQ(font_css("Monospace 10.5"), std::string(".vm-editor {\n"
                                                      "    font-family: \"Monospace\";\n"
                                                      "    font-size: 10.5pt;\n"
                                                      "}\n"));
}

TEST(FontCss, AnAbsoluteSizeIsInPixels) {
    EXPECT_EQ(font_css("Monospace 12px"), std::string(".vm-editor {\n"
                                                      "    font-family: \"Monospace\";\n"
                                                      "    font-size: 12px;\n"
                                                      "}\n"));
}

TEST(FontCss, WeightAndStyleComeAfterTheSize) {
    EXPECT_EQ(font_css("Sans Bold Italic 12"), std::string(".vm-editor {\n"
                                                           "    font-family: \"Sans\";\n"
                                                           "    font-size: 12pt;\n"
                                                           "    font-weight: 700;\n"
                                                           "    font-style: italic;\n"
                                                           "}\n"));
}

TEST(FontCss, ALightObliqueFont) {
    EXPECT_EQ(font_css("Sans Light Oblique 9"), std::string(".vm-editor {\n"
                                                            "    font-family: \"Sans\";\n"
                                                            "    font-size: 9pt;\n"
                                                            "    font-weight: 300;\n"
                                                            "    font-style: oblique;\n"
                                                            "}\n"));
}

TEST(FontCss, OnlyTheFieldsThatAreSetAreWritten) {
    EXPECT_EQ(font_css("Monospace"), std::string(".vm-editor {\n"
                                                 "    font-family: \"Monospace\";\n"
                                                 "}\n"));
    EXPECT_EQ(font_css("Bold 12"), std::string(".vm-editor {\n"
                                               "    font-size: 12pt;\n"
                                               "    font-weight: 700;\n"
                                               "}\n"));
}

TEST(FontCss, EveryNameOfAFamilyListIsQuoted) {
    EXPECT_EQ(font_css("DejaVu Sans Mono, Monospace 11"),
              std::string(".vm-editor {\n"
                          "    font-family: \"DejaVu Sans Mono\", \"Monospace\";\n"
                          "    font-size: 11pt;\n"
                          "}\n"));
}

TEST(FontCss, EmptyNamesInTheListAreLeftOut) {
    EXPECT_EQ(font_css("Mono,, Sans, 10"), std::string(".vm-editor {\n"
                                                       "    font-family: \"Mono\", \"Sans\";\n"
                                                       "    font-size: 10pt;\n"
                                                       "}\n"));
}

TEST(FontCss, QuotesAndBackslashesInANameAreEscaped) {
    EXPECT_EQ(font_css("Ev\"il\\Font 11"), std::string(".vm-editor {\n"
                                                       "    font-family: \"Ev\\\"il\\\\Font\";\n"
                                                       "    font-size: 11pt;\n"
                                                       "}\n"));
}

TEST(FontCss, ControlCharactersInANameBecomeSpaces) {
    EXPECT_EQ(font_css("A\x01"
                       "B 11"),
              std::string(".vm-editor {\n"
                          "    font-family: \"A B\";\n"
                          "    font-size: 11pt;\n"
                          "}\n"));
}

TEST(FontCss, ANameCannotCloseTheRuleOrTheString) {
    const std::string css = font_css("a}b;c{ 11");
    EXPECT_EQ(css, std::string(".vm-editor {\n"
                               "    font-family: \"a}b;c{\";\n"
                               "    font-size: 11pt;\n"
                               "}\n"));
}

TEST(FontCss, ANormalWeightAndStyleAreNotWritten) {
    EXPECT_EQ(font_css("Monospace Normal 11"), std::string(".vm-editor {\n"
                                                           "    font-family: \"Monospace\";\n"
                                                           "    font-size: 11pt;\n"
                                                           "}\n"));
}

TEST(FontCss, ASizeOfZeroIsLeftOut) {
    EXPECT_EQ(font_css("Sans Bold 0"), std::string(".vm-editor {\n"
                                                   "    font-family: \"Sans\";\n"
                                                   "    font-weight: 700;\n"
                                                   "}\n"));
}

TEST(FontCss, TheSizeKeepsAtMostTwoDecimals) {
    EXPECT_EQ(font_css("Sans 10.333"), std::string(".vm-editor {\n"
                                                   "    font-family: \"Sans\";\n"
                                                   "    font-size: 10.33pt;\n"
                                                   "}\n"));
}

TEST(FontCss, TheDecimalSeparatorIsADotInEveryLocale) {
    const char* before = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = before != nullptr ? before : "C";
    const char* german = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
    if (german == nullptr) {
        std::setlocale(LC_NUMERIC, saved.c_str());
        GTEST_SKIP();
    }
    const std::string css = font_css("Monospace 10.5");
    std::setlocale(LC_NUMERIC, saved.c_str());
    EXPECT_NE(css.find("font-size: 10.5pt;"), std::string::npos);
}

} // namespace
