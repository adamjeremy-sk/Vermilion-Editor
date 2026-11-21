// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of AppearanceHost on real parts: the style, the font style and the tabs, on a display,
// alone and driven by the AppearanceController.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "core/colour_mode.h"
#include "core/document_list.h"
#include "core/editor_look.h"
#include "core/editor_settings.h"
#include "ui/appearance_controller.h"
#include "ui/appearance_host.h"
#include "ui/debounce_timer.h"
#include "ui/document_tabs.h"
#include "ui/editor_page.h"
#include "ui/font_style.h"
#include "ui/style.h"
#include "vermilion_resources.h"

namespace {

namespace fs = std::filesystem;
using vermilion::ColourMode;
using vermilion::DocumentList;
using vermilion::EditorLook;
using vermilion::EditorSettings;
using vermilion::ui::AppearanceController;
using vermilion::ui::AppearanceHost;
using vermilion::ui::DebounceTimer;
using vermilion::ui::DocumentTabs;
using vermilion::ui::FontStyle;
using vermilion::ui::Style;

constexpr char kDarkScheme[] = "vermilion-classic-dark";

bool g_display = false;

std::string& cache_root() {
    static std::string root;
    return root;
}

// A timer that never fires: the controller's start() applies at once.
class IdleTimer : public DebounceTimer {
public:
    void start(unsigned, std::function<void()>) override {}
    void stop() override {}
};

// Every test needs GTK on a real display; without one (a build server) they are skipped.
class HostTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gchar* made = g_dir_make_tmp("vermilion-host-XXXXXX", nullptr);
        std::unique_ptr<gchar, decltype(&g_free)> folder(made, &g_free);
        if (folder) {
            cache_root() = folder.get();
            g_setenv("XDG_CACHE_HOME", cache_root().c_str(), TRUE);
        }
        g_display = gtk_init_check();
        if (g_display) {
            gtk_source_init();
            vermilion_register_resource();
        }
    }

    static void TearDownTestSuite() {
        std::error_code error;
        fs::remove_all(cache_root(), error);
    }

    // The tabs are made here, after the display check: GTK widgets need a display.
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        list_ = std::make_unique<DocumentList>();
        tabs_ = std::make_unique<DocumentTabs>(*list_, tabs_callbacks());
    }

    void TearDown() override {
        tabs_.reset();
        list_.reset();
    }

    static DocumentTabs::Callbacks tabs_callbacks() {
        return DocumentTabs::Callbacks{
            [] {},
            [](vermilion::DocumentId) {},
            [](const std::string&) {},
            [](const std::string&, const std::string&) {},
            [](vermilion::DocumentId, vermilion::ui::DocumentSaver::Outcome, const std::string&) {},
            [](vermilion::DocumentId) {}};
    }

    static std::string scheme_id(const vermilion::ui::EditorPage& page) {
        GtkSourceStyleScheme* scheme =
            gtk_source_buffer_get_style_scheme(page.buffer()); // borrowed
        return scheme != nullptr ? gtk_source_style_scheme_get_id(scheme) : "";
    }

    static EditorLook dark_look() {
        EditorLook look;
        look.dark = true;
        look.tab_width = 8;
        look.font = "Monospace 13";
        return look;
    }

    std::unique_ptr<DocumentList> list_;
    std::unique_ptr<DocumentTabs> tabs_; // after list_, which it uses
};

TEST_F(HostTest, ThePaletteGoesToTheStyle) {
    std::unique_ptr<Style> style = Style::install(false);
    ASSERT_NE(style, nullptr);
    AppearanceHost host(style.get(), nullptr, *tabs_, nullptr);
    host.apply_palette(true);
    EXPECT_TRUE(style->dark());
    host.apply_palette(false);
    EXPECT_FALSE(style->dark());
}

TEST_F(HostTest, TheLookGoesToTheTabsAndTheirPages) {
    tabs_->add_untitled();
    AppearanceHost host(nullptr, nullptr, *tabs_, nullptr);
    host.apply_look(dark_look());
    EXPECT_TRUE(tabs_->look() == dark_look());
    ASSERT_NE(tabs_->active_editor(), nullptr);
    EXPECT_EQ(scheme_id(*tabs_->active_editor()), std::string(kDarkScheme));
}

TEST_F(HostTest, TheFontOfTheLookGoesToTheFontStyle) {
    std::unique_ptr<FontStyle> font = FontStyle::install();
    ASSERT_NE(font, nullptr);
    AppearanceHost host(nullptr, font.get(), *tabs_, nullptr);
    host.apply_look(dark_look());
    EXPECT_EQ(font->font(), std::string("Monospace 13"));
}

TEST_F(HostTest, TheLimitGoesToTheCallback) {
    std::vector<std::uint64_t> limits;
    AppearanceHost host(nullptr, nullptr, *tabs_,
                        [&limits](std::uint64_t limit) { limits.push_back(limit); });
    host.apply_long_line_limit(20000);
    ASSERT_EQ(limits.size(), std::size_t{1});
    EXPECT_EQ(limits[0], std::uint64_t{20000});
}

TEST_F(HostTest, MissingPartsAreLeftAlone) {
    AppearanceHost host(nullptr, nullptr, *tabs_, nullptr);
    host.apply_palette(true);
    host.apply_look(dark_look());
    host.apply_long_line_limit(20000);
    EXPECT_TRUE(tabs_->look() == dark_look());
}

TEST_F(HostTest, TheControllerDrivesEveryPart) {
    std::unique_ptr<Style> style = Style::install(false);
    std::unique_ptr<FontStyle> font = FontStyle::install();
    ASSERT_NE(style, nullptr);
    ASSERT_NE(font, nullptr);
    std::vector<std::uint64_t> limits;
    AppearanceHost host(style.get(), font.get(), *tabs_,
                        [&limits](std::uint64_t limit) { limits.push_back(limit); });
    AppearanceController controller(host, std::make_unique<IdleTimer>());
    EditorSettings values;
    values.colour_mode = ColourMode::Dark;
    values.tab_width = 8;
    values.font = "Monospace 13";
    values.highlight_current_line = false;
    values.long_line_limit = 20000;
    controller.start(values);
    EXPECT_TRUE(style->dark());
    EXPECT_TRUE(tabs_->look().dark);
    EXPECT_EQ(tabs_->look().tab_width, 8);
    EXPECT_FALSE(tabs_->look().highlight_current_line);
    EXPECT_EQ(font->font(), std::string("Monospace 13"));
    ASSERT_EQ(limits.size(), std::size_t{1});
    EXPECT_EQ(limits[0], std::uint64_t{20000});
    tabs_->add_untitled();
    ASSERT_NE(tabs_->active_editor(), nullptr);
    EXPECT_EQ(scheme_id(*tabs_->active_editor()), std::string(kDarkScheme));
}

} // namespace
