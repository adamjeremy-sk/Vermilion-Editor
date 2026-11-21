// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the stylesheets, the menu bar and the icon are compiled into the program's resources.

#include <gtest/gtest.h>

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

TEST(UiResources, HelpWindowHasItsRules) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-help scrolledwindow.vm-help-well"), std::string::npos);
    EXPECT_NE(css.find("window.vm-help label.vm-help-h1"), std::string::npos);
    EXPECT_NE(css.find("window.vm-help grid.vm-help-table"), std::string::npos);
    EXPECT_NE(css.find("window.vm-help .vm-help-page label link"), std::string::npos);
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

} // namespace
