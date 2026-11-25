// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the Appearance tab on a real display (the tests skip without one): what it shows for
// the settings, what it stores back, when it says that the person changed something, and the
// monospace filter of its font dialog.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <memory>
#include <string>

#include "core/colour_mode.h"
#include "core/editor_settings.h"
#include "ui/settings_page_appearance.h"

namespace {

using vermilion::ColourMode;
using vermilion::EditorSettings;
using vermilion::ui::AppearancePage;

bool g_display = false;

// The families of the default font map: one monospace and one proportional family are needed.
struct Families {
    PangoFontFamily* mono = nullptr;
    PangoFontFamily* proportional = nullptr;
    PangoFontFamily** list = nullptr;
    int count = 0;

    Families() {
        pango_font_map_list_families(pango_cairo_font_map_get_default(), &list, &count);
        for (int i = 0; i < count; ++i) {
            if (pango_font_family_is_monospace(list[i])) {
                mono = mono == nullptr ? list[i] : mono;
            } else {
                proportional = proportional == nullptr ? list[i] : proportional;
            }
        }
    }
    ~Families() { g_free(list); } // the array only; the families belong to the font map
    Families(const Families&) = delete;
    Families& operator=(const Families&) = delete;
};

class AppearancePageTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { g_display = gtk_init_check(); }

    // The page is made here, after the display check: GTK widgets need a display. A window holds
    // its widget so that nothing is left floating.
    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        pick_fonts();
        if (font_a_.empty() || font_b_.empty()) {
            GTEST_SKIP() << "the system has fewer than two plain font families";
        }
        window_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        page_ = std::make_unique<AppearancePage>();
        gtk_window_set_child(window_, page_->widget());
        page_->set_changed([this] { ++changes_; });
    }

    void TearDown() override {
        page_.reset();
        if (window_ != nullptr) {
            gtk_window_destroy(window_);
        }
    }

    // Two real font families (letters and spaces only): GTK reports a critical for a family that
    // is not installed, so the tests do not invent one.
    void pick_fonts() {
        Families families;
        for (int i = 0; i < families.count; ++i) {
            const std::string name = pango_font_family_get_name(families.list[i]);
            if (name.empty() ||
                name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ") !=
                    std::string::npos) {
                continue;
            }
            if (font_a_.empty()) {
                font_a_ = name;
            } else if (font_b_.empty() && name != font_a_) {
                font_b_ = name;
            }
        }
    }

    void load(const EditorSettings& settings) { page_->load(settings); }

    // What the page stores into a copy of `base`.
    EditorSettings stored(const EditorSettings& base = EditorSettings{}) const {
        EditorSettings copy = base;
        page_->store(copy);
        return copy;
    }

    unsigned selected() const {
        return gtk_drop_down_get_selected(GTK_DROP_DOWN(page_->colour_dropdown()));
    }
    static bool active(GtkWidget* check) {
        return gtk_check_button_get_active(GTK_CHECK_BUTTON(check)) != FALSE;
    }
    static void set_active(GtkWidget* check, bool on) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), on);
    }
    bool font_enabled() const { return gtk_widget_get_sensitive(page_->font_button()) != FALSE; }
    void choose_font(const char* text) {
        PangoFontDescription* description = pango_font_description_from_string(text);
        gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(page_->font_button()),
                                             description);
        pango_font_description_free(description);
    }

    GtkWindow* window_ = nullptr;
    std::unique_ptr<AppearancePage> page_;
    int changes_ = 0;
    std::string font_a_;
    std::string font_b_;
};

TEST(AppearancePageIndex, EveryColourModeHasItsPlaceInTheDropDown) {
    EXPECT_EQ(AppearancePage::colour_mode_index(ColourMode::Light), 0u);
    EXPECT_EQ(AppearancePage::colour_mode_index(ColourMode::Dark), 1u);
    EXPECT_EQ(AppearancePage::colour_mode_index(ColourMode::System), 2u);
    EXPECT_EQ(AppearancePage::colour_mode_at(0), ColourMode::Light);
    EXPECT_EQ(AppearancePage::colour_mode_at(1), ColourMode::Dark);
    EXPECT_EQ(AppearancePage::colour_mode_at(2), ColourMode::System);
}

TEST(AppearancePageIndex, AnIndexOutsideTheListGivesFollowSystem) {
    EXPECT_EQ(AppearancePage::colour_mode_at(3), ColourMode::System);
    EXPECT_EQ(AppearancePage::colour_mode_at(GTK_INVALID_LIST_POSITION), ColourMode::System);
}

TEST(AppearancePageIndex, ThereIsADefaultFont) {
    EXPECT_EQ(std::string(AppearancePage::default_font_text()), std::string("Monospace 12"));
}

TEST_F(AppearancePageTest, NothingAndNonFontsAreNotMonospace) {
    EXPECT_FALSE(AppearancePage::is_monospace_item(nullptr));
    GtkWidget* label = gtk_label_new("x"); // floating, destroyed with the reference below
    g_object_ref_sink(label);
    EXPECT_FALSE(AppearancePage::is_monospace_item(label));
    g_object_unref(label);
}

TEST_F(AppearancePageTest, ItStartsWithTheSystemFontBoxCheckedAndTheGenericFontGreyedOut) {
    load(vermilion::default_settings());
    EXPECT_EQ(selected(), 2u);
    EXPECT_TRUE(active(page_->highlight_check()));
    EXPECT_TRUE(active(page_->system_font_check()));
    EXPECT_FALSE(font_enabled());
    const PangoFontDescription* shown =
        gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(page_->font_button()));
    ASSERT_NE(shown, nullptr);
    g_autofree char* text = pango_font_description_to_string(shown);
    EXPECT_EQ(std::string(text), std::string("Monospace 12"));
    EXPECT_TRUE(stored().font.empty());
}

TEST_F(AppearancePageTest, UncheckingTheBoxOfTheDefaultsSwitchesToTheGenericFontFirst) {
    load(vermilion::default_settings());
    set_active(page_->system_font_check(), false);
    EXPECT_TRUE(font_enabled());
    EXPECT_EQ(stored().font, std::string("Monospace 12"));
}

TEST_F(AppearancePageTest, AnEmptyFontShowsTheSystemFontBox) {
    EditorSettings settings;
    settings.font.clear();
    load(settings);
    EXPECT_TRUE(active(page_->system_font_check()));
    EXPECT_FALSE(font_enabled());
}

TEST_F(AppearancePageTest, LoadShowsEveryColourMode) {
    EditorSettings settings;
    settings.colour_mode = ColourMode::Light;
    load(settings);
    EXPECT_EQ(selected(), 0u);
    settings.colour_mode = ColourMode::Dark;
    load(settings);
    EXPECT_EQ(selected(), 1u);
    settings.colour_mode = ColourMode::System;
    load(settings);
    EXPECT_EQ(selected(), 2u);
}

TEST_F(AppearancePageTest, LoadShowsTheCurrentLineSetting) {
    EditorSettings settings;
    settings.highlight_current_line = false;
    load(settings);
    EXPECT_FALSE(active(page_->highlight_check()));
    settings.highlight_current_line = true;
    load(settings);
    EXPECT_TRUE(active(page_->highlight_check()));
}

TEST_F(AppearancePageTest, LoadShowsAFontOfItsOwn) {
    EditorSettings settings;
    settings.font = font_a_ + " 11";
    load(settings);
    EXPECT_FALSE(active(page_->system_font_check()));
    EXPECT_TRUE(font_enabled());
    EXPECT_EQ(stored().font, font_a_ + " 11");
}

TEST_F(AppearancePageTest, StoreRoundTripsWhatWasLoaded) {
    EditorSettings settings;
    settings.colour_mode = ColourMode::Dark;
    settings.highlight_current_line = false;
    settings.font = font_a_ + " Bold 9";
    load(settings);
    const EditorSettings back = stored();
    EXPECT_EQ(back.colour_mode, ColourMode::Dark);
    EXPECT_FALSE(back.highlight_current_line);
    EXPECT_EQ(back.font, font_a_ + " Bold 9");
}

TEST_F(AppearancePageTest, StoreLeavesTheOtherFieldsAlone) {
    EditorSettings base;
    base.tab_width = 8;
    base.confirm_large_reload = false;
    base.long_line_limit = 20000;
    load(base);
    set_active(page_->highlight_check(), false);
    const EditorSettings back = stored(base);
    EXPECT_EQ(back.tab_width, 8);
    EXPECT_FALSE(back.confirm_large_reload);
    EXPECT_EQ(back.long_line_limit, std::uint64_t{20000});
    EXPECT_FALSE(back.highlight_current_line);
}

TEST_F(AppearancePageTest, ChoosingAColourModeIsStored) {
    load(EditorSettings{});
    gtk_drop_down_set_selected(GTK_DROP_DOWN(page_->colour_dropdown()), 1);
    EXPECT_EQ(stored().colour_mode, ColourMode::Dark);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(page_->colour_dropdown()), 0);
    EXPECT_EQ(stored().colour_mode, ColourMode::Light);
}

TEST_F(AppearancePageTest, TheSystemFontBoxStoresNoFontAndDisablesTheButton) {
    EditorSettings settings;
    settings.font = font_a_ + " 9";
    load(settings);
    EXPECT_TRUE(font_enabled());
    set_active(page_->system_font_check(), true);
    EXPECT_FALSE(font_enabled());
    EXPECT_TRUE(stored().font.empty());
}

TEST_F(AppearancePageTest, UncheckingTheSystemFontBoxBringsTheFontBack) {
    EditorSettings settings;
    settings.font = font_a_ + " 9";
    load(settings);
    set_active(page_->system_font_check(), true);
    set_active(page_->system_font_check(), false);
    EXPECT_TRUE(font_enabled());
    EXPECT_EQ(stored().font, font_a_ + " 9");
}

TEST_F(AppearancePageTest, WithNoFontOfItsOwnUncheckingGivesTheDefaultFont) {
    EditorSettings settings;
    settings.font.clear();
    load(settings);
    set_active(page_->system_font_check(), false);
    EXPECT_EQ(stored().font, std::string("Monospace 12"));
}

TEST_F(AppearancePageTest, AFontChosenInTheButtonIsStored) {
    EditorSettings settings;
    settings.font = font_a_ + " 9";
    load(settings);
    choose_font((font_b_ + " Bold 12").c_str());
    EXPECT_EQ(stored().font, font_b_ + " Bold 12");
}

TEST_F(AppearancePageTest, AFontThePersonDidNotTouchIsStoredAsItWasRead) {
    // Pango spells the style "Bold"; the file's own text must come back unchanged.
    EditorSettings settings;
    settings.font = font_a_ + " bold 9";
    load(settings);
    const PangoFontDescription* description =
        gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(page_->font_button()));
    ASSERT_NE(description, nullptr);
    char* spelled = pango_font_description_to_string(description);
    EXPECT_NE(std::string(spelled), settings.font);
    g_free(spelled);
    EXPECT_EQ(stored().font, font_a_ + " bold 9");
    set_active(page_->highlight_check(), false);
    EXPECT_EQ(stored().font, font_a_ + " bold 9");
}

TEST_F(AppearancePageTest, LoadingIsNotAChange) {
    EditorSettings settings;
    settings.colour_mode = ColourMode::Dark;
    settings.highlight_current_line = false;
    settings.font = font_a_ + " 9";
    load(settings);
    load(EditorSettings{});
    EXPECT_EQ(changes_, 0);
}

TEST_F(AppearancePageTest, EveryControlTellsWhenThePersonChangesIt) {
    EditorSettings settings;
    settings.font = font_a_ + " 9";
    load(settings);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(page_->colour_dropdown()), 0);
    const int after_mode = changes_;
    EXPECT_GE(after_mode, 1);
    set_active(page_->highlight_check(), false);
    const int after_line = changes_;
    EXPECT_GT(after_line, after_mode);
    set_active(page_->system_font_check(), true);
    const int after_box = changes_;
    EXPECT_GT(after_box, after_line);
    set_active(page_->system_font_check(), false);
    choose_font((font_b_ + " 12").c_str());
    EXPECT_GT(changes_, after_box + 1);
}

TEST_F(AppearancePageTest, OnlyMonospaceFamiliesAndTheirFacesPassTheFilter) {
    Families families;
    if (families.mono == nullptr || families.proportional == nullptr) {
        GTEST_SKIP() << "the system has no monospace and proportional font pair";
    }
    EXPECT_TRUE(AppearancePage::is_monospace_item(families.mono));
    EXPECT_FALSE(AppearancePage::is_monospace_item(families.proportional));
    PangoFontFace* mono_face =
        pango_font_family_get_face(families.mono, nullptr); // borrowed; null means the default face
    PangoFontFace* other_face = pango_font_family_get_face(families.proportional, nullptr);
    ASSERT_NE(mono_face, nullptr);
    ASSERT_NE(other_face, nullptr);
    EXPECT_TRUE(AppearancePage::is_monospace_item(mono_face));
    EXPECT_FALSE(AppearancePage::is_monospace_item(other_face));
}

TEST_F(AppearancePageTest, TheFontDialogUsesTheMonospaceFilter) {
    GtkFontDialog* dialog =
        gtk_font_dialog_button_get_dialog(GTK_FONT_DIALOG_BUTTON(page_->font_button()));
    ASSERT_NE(dialog, nullptr);
    GtkFilter* filter = gtk_font_dialog_get_filter(dialog);
    ASSERT_NE(filter, nullptr);
    Families families;
    if (families.mono == nullptr || families.proportional == nullptr) {
        GTEST_SKIP() << "the system has no monospace and proportional font pair";
    }
    EXPECT_TRUE(gtk_filter_match(filter, families.mono) != FALSE);
    EXPECT_FALSE(gtk_filter_match(filter, families.proportional) != FALSE);
}

} // namespace
