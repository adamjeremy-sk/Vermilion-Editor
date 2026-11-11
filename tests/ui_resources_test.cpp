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

TEST(UiResources, MenuBarUsesWindowActions) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("<menu id=\"menubar\">"), std::string::npos);
    EXPECT_NE(ui.find("win.new-file"), std::string::npos);
    EXPECT_NE(ui.find("win.open"), std::string::npos);
    EXPECT_NE(ui.find("win.word-wrap"), std::string::npos);
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
