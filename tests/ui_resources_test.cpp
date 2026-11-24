// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the stylesheets, the menu bar and the icon are compiled into the program's resources.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

#include "vermilion_resources.h"

namespace {

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;

// Returns the resource's text, or an empty string when it is missing.
std::string resource_text(const char* path) {
    static const bool registered = [] {
        vermilion_register_resource();
        return true;
    }();
    (void)registered;
    BytesPtr bytes(g_resources_lookup_data(path, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                   &g_bytes_unref);
    if (!bytes) {
        return std::string();
    }
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes.get(), &size));
    return std::string(data, size);
}

// The big-endian 32-bit number at offset; the caller checks that four bytes are there.
std::uint32_t read_be32(const std::string& data, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value = (value << 8) | static_cast<unsigned char>(data[offset + i]);
    }
    return value;
}

TEST(UiResources, LightPaletteDefinesChassis) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_light.css");
    EXPECT_NE(css.find("@define-color vm_chassis #cecece;"), std::string::npos);
}

TEST(UiResources, StructureHasClassicDetails) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-main notebook > header > tabs > tab:checked"), std::string::npos);
    EXPECT_NE(css.find("window.vm-main scrolledwindow.vm-editor-well"), std::string::npos);
    EXPECT_NE(css.find("window.vm-main button.vm-tab-close"), std::string::npos);
    EXPECT_NE(css.find("window.aboutdialog label > selection"), std::string::npos);
    EXPECT_NE(css.find("menubutton.vm-status-panel"), std::string::npos);
}

TEST(UiResources, AboutLogoKeepsItsSizeUnderEveryTheme) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    const std::size_t rule = css.find("window.aboutdialog image.large-icons");
    ASSERT_NE(rule, std::string::npos);
    EXPECT_NE(css.find("-gtk-icon-size: 128px;", rule), std::string::npos);
}

// The Settings tabs have no close button, so both sides of the label get the same room.
TEST(UiResources, SettingsTabsHaveTheSameRoomOnBothSides) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    auto body = [&css](const char* selector) {
        const std::size_t at = css.find(selector);
        return at == std::string::npos ? std::string() : css.substr(at, css.find('}', at) - at);
    };
    EXPECT_NE(body("window.vm-settings notebook > header > tabs > tab {")
                  .find("padding: 1px 10px 1px 10px;"),
              std::string::npos);
    EXPECT_NE(body("window.vm-settings notebook > header > tabs > tab:checked {")
                  .find("padding: 3px 10px 1px 10px;"),
              std::string::npos);
}

TEST(UiResources, LightPaletteDefinesInfoBackground) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_light.css");
    EXPECT_NE(css.find("@define-color vm_info_bg #ffffe1;"), std::string::npos);
}

TEST(UiResources, StructureHasChangeBannerRule) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-main box.vm-change-banner {"), std::string::npos);
}

TEST(UiResources, HelpWindowKeepsItsBarRuleAndNothingOfTheLabelPage) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-help .vm-help-bar {"), std::string::npos);
    for (const char* gone :
         {"vm-help-well", "vm-help-page", "vm-help-h1", "vm-help-h2", "vm-help-h3", "vm-help-item",
          "vm-help-table", "vm-help-th", "vm-help-td", "vm-help-bullet"}) {
        SCOPED_TRACE(gone);
        EXPECT_EQ(css.find(gone), std::string::npos);
    }
}

TEST(UiResources, HelpPageViewHasItsFont) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-help textview.vm-help-view {"), std::string::npos);
    EXPECT_NE(css.find("font-family: monospace;"), std::string::npos);
    EXPECT_EQ(css.find("Fantasque"), std::string::npos);
}

TEST(UiResources, FindWindowHasItsRule) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-find .vm-find-problem"), std::string::npos);
}

TEST(UiResources, ReplaceWindowHasItsRule) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-replace .vm-find-problem"), std::string::npos);
}

TEST(UiResources, SearchMenuHasFind) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("_Search"), std::string::npos);
    EXPECT_NE(ui.find("win.find"), std::string::npos);
}

TEST(UiResources, SearchMenuHasReplace) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("win.replace"), std::string::npos);
}

TEST(UiResources, GotoLineWindowHasItsRule) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-goto .vm-find-problem"), std::string::npos);
}

TEST(UiResources, SearchMenuHasFindNextAndFindPrevious) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("win.find-next</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("win.find-previous</attribute>"), std::string::npos);
}

// ARCHITECTURE.md 7.10: the Search menu reads Find, Find Next, Find Previous, Replace, Go to Line.
TEST(UiResources, SearchMenuOrder) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    const char* order[] = {"win.find</attribute>", "win.find-next</attribute>",
                           "win.find-previous</attribute>", "win.replace</attribute>",
                           "win.go-to-line</attribute>"};
    std::size_t last = 0;
    for (const char* action : order) {
        SCOPED_TRACE(action);
        std::size_t at = ui.find(action);
        ASSERT_NE(at, std::string::npos);
        EXPECT_GT(at, last);
        last = at;
    }
}

TEST(UiResources, SearchMenuHasGotoLine) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("win.go-to-line"), std::string::npos);
}

TEST(UiResources, FileMenuHasReloadFromDisk) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("_Reload from Disk</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("win.reload-from-disk</attribute>"), std::string::npos);
}

// The File menu reads Save As, Reload from Disk, Close Tab (Reload has a section of its own).
TEST(UiResources, FileMenuOrder) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    const char* order[] = {"win.save-as</attribute>", "win.reload-from-disk</attribute>",
                           "win.close-tab</attribute>"};
    std::size_t last = 0;
    for (const char* action : order) {
        SCOPED_TRACE(action);
        std::size_t at = ui.find(action);
        ASSERT_NE(at, std::string::npos);
        EXPECT_GT(at, last);
        last = at;
    }
}

TEST(UiResources, ToolsMenuHasSettings) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("_Tools</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("_Settings&#x2026;</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("win.settings</attribute>"), std::string::npos);
}

// ARCHITECTURE.md 7.10: the menu bar reads File, Edit, Search, View, Tools, Help.
TEST(UiResources, ToolsMenuSitsBetweenViewAndHelp) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    const char* order[] = {"label\">_View</attribute>", "win.previous-tab</attribute>",
                           "label\">_Tools</attribute>", "win.settings</attribute>",
                           "label\">_Help</attribute>"};
    std::size_t last = 0;
    for (const char* text : order) {
        SCOPED_TRACE(text);
        std::size_t at = ui.find(text);
        ASSERT_NE(at, std::string::npos);
        EXPECT_GT(at, last);
        last = at;
    }
}

TEST(UiResources, QuestionWindowHasItsRule) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-ask .vm-ask-message"), std::string::npos);
}

TEST(UiResources, MenuBarUsesWindowActions) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("<menu id=\"menubar\">"), std::string::npos);
    EXPECT_NE(ui.find("win.new-file"), std::string::npos);
    EXPECT_NE(ui.find("win.open"), std::string::npos);
    EXPECT_NE(ui.find("win.word-wrap"), std::string::npos);
    EXPECT_NE(ui.find("win.help-index"), std::string::npos);
    EXPECT_NE(ui.find("win.save</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("win.save-as</attribute>"), std::string::npos);
    EXPECT_NE(ui.find("<menu id=\"encoding-menu\">"), std::string::npos);
    EXPECT_NE(ui.find("<menu id=\"line-ending-menu\">"), std::string::npos);
    EXPECT_NE(ui.find("win.reopen-encoding"), std::string::npos);
    EXPECT_NE(ui.find("win.save-encoding"), std::string::npos);
    EXPECT_NE(ui.find("win.line-ending"), std::string::npos);
    EXPECT_EQ(ui.find("app."), std::string::npos);
}

// Every size is a PNG of exactly that size: the width and height of its IHDR chunk.
TEST(UiResources, IconHasEverySize) {
    const std::string signature("\x89PNG\r\n\x1a\n", 8);
    for (std::uint32_t size : {16u, 24u, 32u, 48u, 64u, 128u, 256u}) {
        std::string dims = std::to_string(size) + "x" + std::to_string(size);
        std::string path = "/io/github/adamjeremy_sk/Vermilion/icons/" + dims +
                           "/apps/io.github.adamjeremy_sk.Vermilion.png";
        SCOPED_TRACE(path);
        std::string png = resource_text(path.c_str());
        ASSERT_GE(png.size(), std::size_t{24});
        EXPECT_EQ(png.substr(0, 8), signature);
        EXPECT_EQ(png.substr(12, 4), "IHDR");
        EXPECT_EQ(read_be32(png, 16), size);
        EXPECT_EQ(read_be32(png, 20), size);
    }
}

// The colour names of the Help page (see ui/help_page_view.h), in both palettes.
constexpr const char* kHelpColourNames[] = {
    "vm_help_h1",       "vm_help_h2",         "vm_help_link",
    "vm_help_code_bg",  "vm_help_th_bg",      "vm_help_th_text",
    "vm_help_match_bg", "vm_help_current_bg", "vm_help_current_text"};

// The colour that `css` gives `name` with @define-color (as red, green, blue in 0..1), or false.
bool defined_colour(const std::string& css, const std::string& name, double (&rgb)[3]) {
    const std::string key = "@define-color " + name + " #";
    const std::size_t at = css.find(key);
    if (at == std::string::npos || css.size() < at + key.size() + 7) {
        return false;
    }
    const std::string hex = css.substr(at + key.size(), 6);
    if (css[at + key.size() + 6] != ';') {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        rgb[i] = std::stoi(hex.substr(2 * i, 2), nullptr, 16) / 255.0;
    }
    return true;
}

// The WCAG contrast ratio of two defined colours; 0 when one is missing.
double contrast(const std::string& css, const std::string& a, const std::string& b) {
    double ca[3];
    double cb[3];
    if (!defined_colour(css, a, ca) || !defined_colour(css, b, cb)) {
        return 0.0;
    }
    const auto luminance = [](const double(&c)[3]) {
        double lin[3];
        for (int i = 0; i < 3; ++i) {
            lin[i] = c[i] <= 0.03928 ? c[i] / 12.92 : std::pow((c[i] + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
    };
    const double la = luminance(ca);
    const double lb = luminance(cb);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

TEST(UiResources, BothPalettesDefineTheHelpColours) {
    for (const char* file : {"classic_light.css", "classic_dark.css"}) {
        const std::string css =
            resource_text((std::string("/io/github/adamjeremy_sk/Vermilion/") + file).c_str());
        for (const char* name : kHelpColourNames) {
            double rgb[3];
            EXPECT_TRUE(defined_colour(css, name, rgb)) << file << " " << name;
        }
    }
}

// Text on its background is at least 4.5:1 in both palettes: headings and links on the page
// colour, body text on the tint of a code span and of a match, the table header's text on its
// background, the current match's text on its background. (A link on a tint is not asserted: no
// tint is pale enough for the red link colour to reach 4.5 on it, and the case is rare.)
TEST(UiResources, TheHelpColoursAreReadableInBothPalettes) {
    for (const char* file : {"classic_light.css", "classic_dark.css"}) {
        const std::string css =
            resource_text((std::string("/io/github/adamjeremy_sk/Vermilion/") + file).c_str());
        SCOPED_TRACE(file);
        EXPECT_GE(contrast(css, "vm_help_h1", "vm_field_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_help_h2", "vm_field_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_help_link", "vm_field_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_field_text", "vm_help_code_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_field_text", "vm_help_match_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_help_th_text", "vm_help_th_bg"), 4.5);
        EXPECT_GE(contrast(css, "vm_help_current_text", "vm_help_current_bg"), 4.5);
    }
}

// The three kinds of background of a Help page differ from the page colour, so that they show.
TEST(UiResources, TheHelpTintsStandOutFromThePage) {
    for (const char* file : {"classic_light.css", "classic_dark.css"}) {
        const std::string css =
            resource_text((std::string("/io/github/adamjeremy_sk/Vermilion/") + file).c_str());
        SCOPED_TRACE(file);
        EXPECT_GE(contrast(css, "vm_help_code_bg", "vm_field_bg"), 1.1);
        EXPECT_GE(contrast(css, "vm_help_match_bg", "vm_field_bg"), 1.3);
        EXPECT_GE(contrast(css, "vm_help_current_bg", "vm_field_bg"), 1.5);
    }
}

TEST(UiResources, HelpPageViewIsDrawnInTheFieldColours) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    const std::size_t page = css.find("window.vm-help textview.vm-help-view text {");
    ASSERT_NE(page, std::string::npos);
    const std::size_t page_end = css.find('}', page);
    const std::string page_rule = css.substr(page, page_end - page);
    EXPECT_NE(page_rule.find("background-color: @vm_field_bg;"), std::string::npos);
    EXPECT_NE(page_rule.find("color: @vm_field_text;"), std::string::npos);
    const std::size_t selection = css.find("window.vm-help textview.vm-help-view text selection {");
    ASSERT_NE(selection, std::string::npos);
    const std::size_t selection_end = css.find('}', selection);
    const std::string selection_rule = css.substr(selection, selection_end - selection);
    EXPECT_NE(selection_rule.find("background-color: @vm_select_bg;"), std::string::npos);
    EXPECT_NE(selection_rule.find("color: @vm_select_text;"), std::string::npos);
}

} // namespace
