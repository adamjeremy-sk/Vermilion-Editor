// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests that the stylesheets and the menu bar are compiled into the program's resources.

#include <gtest/gtest.h>

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

TEST(UiResources, LightPaletteDefinesChassis) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_light.css");
    EXPECT_NE(css.find("@define-color vm_chassis #cecece;"), std::string::npos);
}

TEST(UiResources, StructureHasClassicDetails) {
    std::string css = resource_text("/io/github/adamjeremy_sk/Vermilion/classic_structure.css");
    EXPECT_NE(css.find("window.vm-main notebook > header > tabs > tab:checked"), std::string::npos);
    EXPECT_NE(css.find("window.vm-main scrolledwindow.vm-editor-well"), std::string::npos);
    EXPECT_NE(css.find("window.vm-main button.vm-tab-close"), std::string::npos);
}

TEST(UiResources, MenuBarUsesWindowActions) {
    std::string ui = resource_text("/io/github/adamjeremy_sk/Vermilion/menus.ui");
    EXPECT_NE(ui.find("<menu id=\"menubar\">"), std::string::npos);
    EXPECT_NE(ui.find("win.new-file"), std::string::npos);
    EXPECT_EQ(ui.find("app."), std::string::npos);
}

} // namespace
