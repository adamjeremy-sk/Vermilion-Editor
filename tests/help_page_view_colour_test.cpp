// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpPageView on a real display: the palette colours of the tags.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <cstddef>
#include <string>

#include "core/manual_text.h"
#include "core/search_options.h"
#include "help_page_view_test_util.h"
#include "ui/help_page_view.h"

namespace {

using namespace vermilion::ui::help_test;
using vermilion::ManualText;

// A palette with every name the tags read in one colour per name (0 the link, 1 the code span, 2
// and 3 the background and the text of a table header, 4 a match, 5 and 6 the background and the
// text of the current match); `shift` changes them all.
std::string palette_css(int shift) {
    const char* names[] = {"vm_link",     "vm_light",     "vm_chassis",    "vm_field_text",
                           "vm_match_bg", "vm_select_bg", "vm_select_text"};
    std::string css;
    int n = 0;
    for (const char* name : names) {
        char hex[16];
        g_snprintf(hex, sizeof hex, "#%02x%02x%02x", 16 * (n + 1) + shift, 40 + n, 200 - n);
        css += std::string("@define-color ") + name + " " + hex + ";\n";
        ++n;
    }
    return css;
}

// The colours that palette_css(shift) gives the name at position `n`.
GdkRGBA palette_colour(int n, int shift) {
    GdkRGBA colour;
    char hex[16];
    g_snprintf(hex, sizeof hex, "#%02x%02x%02x", 16 * (n + 1) + shift, 40 + n, 200 - n);
    EXPECT_TRUE(gdk_rgba_parse(&colour, hex));
    return colour;
}

// A stylesheet with the palette on the display for as long as the object lives.
class PaletteOnDisplay {
public:
    explicit PaletteOnDisplay(int shift) : provider_(gtk_css_provider_new()) {
        load(shift);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                   GTK_STYLE_PROVIDER(provider_),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    ~PaletteOnDisplay() {
        gtk_style_context_remove_provider_for_display(gdk_display_get_default(),
                                                      GTK_STYLE_PROVIDER(provider_));
        g_object_unref(provider_);
    }
    PaletteOnDisplay(const PaletteOnDisplay&) = delete;
    PaletteOnDisplay& operator=(const PaletteOnDisplay&) = delete;
    // Replaces the palette, as the program's Style does when the colour mode changes.
    void load(int shift) {
        gtk_css_provider_load_from_string(provider_, palette_css(shift).c_str());
    }

private:
    GtkCssProvider* provider_;
};

// The colour property `property` ("foreground" or "background") of the tag; false when unset.
bool tag_colour(const Shown& shown, const char* tag_name, const char* property, GdkRGBA& out) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(shown.text());
    GtkTextTag* tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), tag_name);
    EXPECT_NE(tag, nullptr) << tag_name;
    const std::string rgba = std::string(property) + "-rgba";
    const std::string is_set = std::string(property) + "-set";
    gboolean set = FALSE;
    GdkRGBA* colour = nullptr;
    g_object_get(tag, is_set.c_str(), &set, rgba.c_str(), &colour, nullptr);
    if (colour != nullptr) {
        out = *colour;
        gdk_rgba_free(colour);
    }
    return set != FALSE;
}

void expect_colour(const Shown& shown, const char* tag, const char* property, int n, int shift) {
    GdkRGBA got;
    ASSERT_TRUE(tag_colour(shown, tag, property, got)) << tag << " " << property;
    const GdkRGBA want = palette_colour(n, shift);
    EXPECT_TRUE(gdk_rgba_equal(&got, &want))
        << tag << " " << property << ": " << std::string(gdk_rgba_to_string(&got)) << " want "
        << std::string(gdk_rgba_to_string(&want));
}

// The colour property of the highest-priority tag at `offset` that sets it (GTK lets the tag with
// the highest priority win); false when no tag there sets it.
bool colour_at(const Shown& shown, std::size_t offset, const char* property, GdkRGBA& out) {
    GtkTextIter at;
    gtk_text_buffer_get_iter_at_offset(gtk_text_view_get_buffer(shown.text()), &at,
                                       static_cast<int>(offset));
    const std::string rgba = std::string(property) + "-rgba";
    const std::string is_set = std::string(property) + "-set";
    bool found = false;
    GSList* tags = gtk_text_iter_get_tags(&at); // lowest priority first
    for (GSList* node = tags; node != nullptr; node = node->next) {
        gboolean set = FALSE;
        GdkRGBA* colour = nullptr;
        g_object_get(node->data, is_set.c_str(), &set, rgba.c_str(), &colour, nullptr);
        if (set != FALSE && colour != nullptr) {
            out = *colour;
            found = true;
        }
        if (colour != nullptr) {
            gdk_rgba_free(colour);
        }
    }
    g_slist_free(tags);
    return found;
}

TEST_F(HelpPageViewTest, TheTagsTakeTheirColoursFromThePalette) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    expect_colour(shown, vermilion::ui::kTagLink, "foreground", 0, 0);
    expect_colour(shown, vermilion::ui::kTagCode, "background", 1, 0);
    expect_colour(shown, vermilion::ui::kTagTableHeader, "background", 2, 0);
    expect_colour(shown, vermilion::ui::kTagTableHeader, "foreground", 3, 0);
    GdkRGBA unused;
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagHeading1, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagHeading2, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagHeading3, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagBody, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagBold, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagLink, "background", unused));
}

TEST_F(HelpPageViewTest, RefreshColoursReadsTheNewPalette) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    shown.view->show(build(kMarkdown));
    palette.load(5);
    shown.view->refresh_colours();
    expect_colour(shown, vermilion::ui::kTagLink, "foreground", 0, 5);
    expect_colour(shown, vermilion::ui::kTagCode, "background", 1, 5);
    expect_colour(shown, vermilion::ui::kTagTableHeader, "background", 2, 5);
    expect_colour(shown, vermilion::ui::kTagTableHeader, "foreground", 3, 5);
}

TEST_F(HelpPageViewTest, TheMatchTagsTakeTheirColoursFromThePalette) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    expect_colour(shown, vermilion::ui::kTagMatch, "background", 4, 0);
    expect_colour(shown, vermilion::ui::kTagCurrentMatch, "background", 5, 0);
    expect_colour(shown, vermilion::ui::kTagCurrentMatch, "foreground", 6, 0);
    GdkRGBA unused;
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagMatch, "foreground", unused));
    palette.load(5);
    shown.view->refresh_colours();
    expect_colour(shown, vermilion::ui::kTagMatch, "background", 4, 5);
    expect_colour(shown, vermilion::ui::kTagCurrentMatch, "background", 5, 5);
    expect_colour(shown, vermilion::ui::kTagCurrentMatch, "foreground", 6, 5);
}

// A match wins over the colours of a code span and a link; the current match also over the link's
// text colour.
TEST_F(HelpPageViewTest, AMatchOverridesTheColoursOfCodeAndLinks) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    ManualText page = build("Plain `code` and [link](a.md) text.\n");
    ASSERT_EQ(page.code.size(), std::size_t{1});
    ASSERT_EQ(page.links.size(), std::size_t{1});
    shown.view->show(page);
    vermilion::SearchOptions options;
    options.text = "code|link";
    options.regex = true;
    ASSERT_EQ(shown.view->set_search(options), std::size_t{2});
    ASSERT_TRUE(shown.view->step(true).has_value()); // the code span
    GdkRGBA got;
    const GdkRGBA match_bg = palette_colour(4, 0);
    const GdkRGBA current_bg = palette_colour(5, 0);
    const GdkRGBA current_text = palette_colour(6, 0);
    const GdkRGBA link_text = palette_colour(0, 0);
    ASSERT_TRUE(colour_at(shown, page.code[0].begin, "background", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &current_bg));
    ASSERT_TRUE(colour_at(shown, page.code[0].begin, "foreground", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &current_text));
    ASSERT_TRUE(colour_at(shown, page.links[0].begin, "background", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &match_bg));
    ASSERT_TRUE(colour_at(shown, page.links[0].begin, "foreground", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &link_text));
    ASSERT_TRUE(shown.view->step(true).has_value()); // the link
    ASSERT_TRUE(colour_at(shown, page.links[0].begin, "background", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &current_bg));
    ASSERT_TRUE(colour_at(shown, page.links[0].begin, "foreground", got));
    EXPECT_TRUE(gdk_rgba_equal(&got, &current_text));
}

TEST_F(HelpPageViewTest, WithoutAPaletteTheColoursAreUnset) {
    Shown shown(900, 500);
    GdkRGBA unused;
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagLink, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagCode, "background", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagTableHeader, "background", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagTableHeader, "foreground", unused));
}

TEST_F(HelpPageViewTest, APaletteThatDisappearsLeavesTheColoursUnset) {
    Shown shown(900, 500);
    {
        PaletteOnDisplay palette(0);
        shown.view->refresh_colours();
        GdkRGBA colour;
        EXPECT_TRUE(tag_colour(shown, vermilion::ui::kTagLink, "foreground", colour));
    }
    shown.view->refresh_colours();
    GdkRGBA unused;
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagLink, "foreground", unused));
    EXPECT_FALSE(tag_colour(shown, vermilion::ui::kTagTableHeader, "background", unused));
}

TEST_F(HelpPageViewTest, TheConstructorReadsThePalette) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500, false);
    expect_colour(shown, vermilion::ui::kTagLink, "foreground", 0, 0);
    expect_colour(shown, vermilion::ui::kTagCode, "background", 1, 0);
}

TEST_F(HelpPageViewTest, MappingTheViewReadsThePalette) {
    Shown shown(900, 500, false);
    PaletteOnDisplay palette(0);
    gtk_window_present(GTK_WINDOW(shown.window));
    spin(300);
    expect_colour(shown, vermilion::ui::kTagLink, "foreground", 0, 0);
    expect_colour(shown, vermilion::ui::kTagCode, "background", 1, 0);
}

// Inside a table header the header's colours win over those of a code span and a link.
TEST_F(HelpPageViewTest, ATableHeaderKeepsItsColoursAroundCodeAndLinks) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    ManualText page = build("| `Code` | [Link](a.md) |\n|---|---|\n| one | two |\n");
    ASSERT_EQ(page.code.size(), std::size_t{1});
    ASSERT_EQ(page.links.size(), std::size_t{1});
    shown.view->show(page);
    const GdkRGBA text = palette_colour(3, 0);
    const GdkRGBA background = palette_colour(2, 0);
    for (std::size_t offset : {page.code[0].begin, page.links[0].begin}) {
        GdkRGBA foreground;
        GdkRGBA fill;
        ASSERT_TRUE(colour_at(shown, offset, "foreground", foreground)) << offset;
        ASSERT_TRUE(colour_at(shown, offset, "background", fill)) << offset;
        EXPECT_TRUE(gdk_rgba_equal(&foreground, &text)) << offset;
        EXPECT_TRUE(gdk_rgba_equal(&fill, &background)) << offset;
    }
}

// Outside a table header the colours of a link and of a code span show.
TEST_F(HelpPageViewTest, ALinkAndACodeSpanShowTheirOwnColours) {
    PaletteOnDisplay palette(0);
    Shown shown(900, 500);
    ManualText page = build(kMarkdown);
    shown.view->show(page);
    GdkRGBA link;
    GdkRGBA code;
    ASSERT_TRUE(colour_at(shown, page.links[0].begin, "foreground", link));
    ASSERT_TRUE(colour_at(shown, page.code[0].begin, "background", code));
    const GdkRGBA want_link = palette_colour(0, 0);
    const GdkRGBA want_code = palette_colour(1, 0);
    EXPECT_TRUE(gdk_rgba_equal(&link, &want_link));
    EXPECT_TRUE(gdk_rgba_equal(&code, &want_code));
}

} // namespace
