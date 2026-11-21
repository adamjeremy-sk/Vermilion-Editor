// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of the look of the editor pages on a real display: EditorPage::apply_look, and
// DocumentTabs::set_look giving it to the pages that are open and the ones made later.

#include <glib.h>
#include <gtest/gtest.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "core/document_list.h"
#include "core/editor_look.h"
#include "ui/document_tabs.h"
#include "ui/editor_page.h"
#include "ui/font_css.h"
#include "vermilion_resources.h"

namespace {

namespace fs = std::filesystem;
using vermilion::DocumentList;
using vermilion::EditorLook;
using vermilion::ui::DocumentTabs;
using vermilion::ui::EditorPage;
using vermilion::ui::kEditorFontClass;

constexpr char kLightScheme[] = "vermilion-classic";
constexpr char kDarkScheme[] = "vermilion-classic-dark";

bool g_display = false;

// The test's own cache folder: XDG_CACHE_HOME points here before GLib is asked for the cache
// folder, so the tests never write into the real cache of the person who runs them.
std::string& cache_root() {
    static std::string root;
    return root;
}

// Every test needs GTK on a real display; without one (a build server) they are skipped.
class LookTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gchar* made = g_dir_make_tmp("vermilion-look-XXXXXX", nullptr);
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

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
    }

    static EditorPage::Callbacks quiet_callbacks() {
        return EditorPage::Callbacks{
            [] {},       [](int) {},
            [](bool) {}, [] {},
            [](int) {},  [](vermilion::ui::DocumentLoader::Outcome, const std::string&) {},
            [](int) {},  [](vermilion::ui::DocumentSaver::Outcome, const std::string&) {},
            [] {}};
    }

    static DocumentTabs::Callbacks tabs_callbacks(int* changes) {
        return DocumentTabs::Callbacks{
            [changes] { ++*changes; },
            [](vermilion::DocumentId) {},
            [](const std::string&) {},
            [](const std::string&, const std::string&) {},
            [](vermilion::DocumentId, vermilion::ui::DocumentSaver::Outcome, const std::string&) {},
            [](vermilion::DocumentId) {}};
    }

    static std::string scheme_id(const EditorPage& page) {
        GtkSourceStyleScheme* scheme =
            gtk_source_buffer_get_style_scheme(page.buffer()); // borrowed
        return scheme != nullptr ? gtk_source_style_scheme_get_id(scheme) : "";
    }

    static GtkSourceView* source_view(const EditorPage& page) {
        return GTK_SOURCE_VIEW(page.view());
    }

    static EditorLook dark_look() {
        EditorLook look;
        look.dark = true;
        look.highlight_current_line = false;
        look.tab_width = 8;
        return look;
    }
};

TEST_F(LookTest, ANewPageHasTheDefaultLook) {
    EditorPage page(quiet_callbacks());
    EXPECT_EQ(scheme_id(page), std::string(kLightScheme));
    EXPECT_TRUE(gtk_source_view_get_highlight_current_line(source_view(page)));
    EXPECT_EQ(gtk_source_view_get_tab_width(source_view(page)), 4u);
    EXPECT_TRUE(gtk_source_view_get_insert_spaces_instead_of_tabs(source_view(page)));
    EXPECT_TRUE(gtk_widget_has_css_class(page.view(), kEditorFontClass));
}

TEST_F(LookTest, TheDarkLookChangesTheSchemeTheLineAndTheTab) {
    EditorPage page(quiet_callbacks());
    page.apply_look(dark_look());
    EXPECT_EQ(scheme_id(page), std::string(kDarkScheme));
    EXPECT_FALSE(gtk_source_view_get_highlight_current_line(source_view(page)));
    EXPECT_EQ(gtk_source_view_get_tab_width(source_view(page)), 8u);
    EXPECT_TRUE(gtk_source_view_get_insert_spaces_instead_of_tabs(source_view(page)));
}

TEST_F(LookTest, TheLightLookComesBack) {
    EditorPage page(quiet_callbacks());
    page.apply_look(dark_look());
    page.apply_look(EditorLook{});
    EXPECT_EQ(scheme_id(page), std::string(kLightScheme));
    EXPECT_TRUE(gtk_source_view_get_highlight_current_line(source_view(page)));
    EXPECT_EQ(gtk_source_view_get_tab_width(source_view(page)), 4u);
}

TEST_F(LookTest, TheClassIsOnTheTextViewOnceAndOnlyThere) {
    EditorPage page(quiet_callbacks());
    page.apply_look(dark_look());
    page.apply_look(dark_look());
    EXPECT_TRUE(gtk_widget_has_css_class(page.view(), kEditorFontClass));
    EXPECT_FALSE(gtk_widget_has_css_class(page.widget(), kEditorFontClass));
}

TEST_F(LookTest, TheTextTheCursorAndTheModifiedFlagStay) {
    EditorPage page(quiet_callbacks());
    GtkTextBuffer* buffer = GTK_TEXT_BUFFER(page.buffer()); // borrowed
    gtk_text_buffer_set_text(buffer, "one\ntwo\nthree", -1);
    gtk_text_buffer_set_modified(buffer, FALSE);
    GtkTextIter place;
    gtk_text_buffer_get_iter_at_offset(buffer, &place, 6);
    gtk_text_buffer_place_cursor(buffer, &place);
    page.apply_look(dark_look());
    EXPECT_EQ(page.text(), std::string("one\ntwo\nthree"));
    EXPECT_FALSE(gtk_text_buffer_get_modified(buffer));
    gtk_text_buffer_get_iter_at_mark(buffer, &place, gtk_text_buffer_get_insert(buffer));
    EXPECT_EQ(gtk_text_iter_get_offset(&place), 6);
}

TEST_F(LookTest, TheTabsHaveTheDefaultLookAtFirst) {
    DocumentList list;
    int changes = 0;
    DocumentTabs tabs(list, tabs_callbacks(&changes));
    EXPECT_TRUE(tabs.look() == EditorLook{});
    tabs.add_untitled();
    ASSERT_NE(tabs.active_editor(), nullptr);
    EXPECT_EQ(scheme_id(*tabs.active_editor()), std::string(kLightScheme));
}

TEST_F(LookTest, SetLookReachesTheOpenPages) {
    DocumentList list;
    int changes = 0;
    DocumentTabs tabs(list, tabs_callbacks(&changes));
    tabs.add_untitled();
    tabs.add_untitled();
    tabs.set_look(dark_look());
    EXPECT_TRUE(tabs.look() == dark_look());
    ASSERT_EQ(list.records().size(), std::size_t{2});
    for (const vermilion::DocumentRecord& record : list.records()) {
        EditorPage* page = tabs.editor(record.id); // borrowed
        ASSERT_NE(page, nullptr);
        EXPECT_EQ(scheme_id(*page), std::string(kDarkScheme));
        EXPECT_EQ(gtk_source_view_get_tab_width(source_view(*page)), 8u);
        EXPECT_FALSE(gtk_source_view_get_highlight_current_line(source_view(*page)));
    }
}

TEST_F(LookTest, APageMadeLaterHasTheLookToo) {
    DocumentList list;
    int changes = 0;
    DocumentTabs tabs(list, tabs_callbacks(&changes));
    tabs.set_look(dark_look());
    tabs.add_untitled();
    ASSERT_NE(tabs.active_editor(), nullptr);
    EXPECT_EQ(scheme_id(*tabs.active_editor()), std::string(kDarkScheme));
    EXPECT_EQ(gtk_source_view_get_tab_width(source_view(*tabs.active_editor())), 8u);
}

TEST_F(LookTest, SetLookTellsTheWindow) {
    DocumentList list;
    int changes = 0;
    DocumentTabs tabs(list, tabs_callbacks(&changes));
    tabs.add_untitled();
    changes = 0;
    tabs.set_look(dark_look());
    EXPECT_EQ(changes, 1);
}

TEST_F(LookTest, SetLookWithoutPagesIsFine) {
    DocumentList list;
    int changes = 0;
    DocumentTabs tabs(list, tabs_callbacks(&changes));
    tabs.set_look(dark_look());
    EXPECT_TRUE(tabs.look() == dark_look());
}

} // namespace
