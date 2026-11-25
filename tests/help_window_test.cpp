// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests of HelpWindow on a real display (the tests skip without one): the page view and the
// Contents tree in the window, links, Back, the size at which it opens, and the colours after a
// palette change.

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_results.h"
#include "core/manual_search.h"
#include "core/manual_spans.h"
#include "core/manual_text.h"
#include "core/search_messages.h"
#include "core/search_options.h"
#include "manual_test_columns.h"
#include "ui/help_page_view.h"
#include "ui/help_search.h"
#include "ui/help_window.h"
#include "ui/manual_library.h"
#include "ui/style.h"
#include "vermilion_manual.h"
#include "vermilion_resources.h"

namespace {

using vermilion::ManualText;
using vermilion::ui::HelpWindow;
using vermilion::ui::ManualLibrary;

bool g_display = false;

// The text that the page view shows for a topic file of the manual folder.
ManualText page_of_file(const std::string& name, std::size_t columns = kManualTestColumns) {
    std::ifstream file(std::string(VERMILION_MANUAL_DIR) + "/" + name);
    std::stringstream text;
    text << file.rdbuf();
    return vermilion::build_manual_text(vermilion::parse_manual(text.str()), columns);
}

// Runs the main loop for about `ms` milliseconds.
void spin(int ms) {
    gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

// Runs the main loop until `library` is ready, for at most ten seconds.
void wait_ready(ManualLibrary& library) {
    const gint64 end = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!library.ready() && g_get_monotonic_time() < end) {
        g_main_context_iteration(nullptr, FALSE);
        g_usleep(1000);
    }
}

// A palette with every colour name the Help page tags read (the link first); `shift` changes them
// all.
std::string palette_css(int shift) {
    const char* names[] = {"vm_link", "vm_light", "vm_chassis", "vm_field_text"};
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

// The foreground colour of the link tag, as text ("" when it is not set).
std::string link_colour(HelpWindow& help) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(help.page_view().text_view());
    GtkTextTag* tag =
        gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), vermilion::ui::kTagLink);
    gboolean set = FALSE;
    GdkRGBA* colour = nullptr;
    g_object_get(tag, "foreground-set", &set, "foreground-rgba", &colour, nullptr);
    std::string text;
    if (set != FALSE && colour != nullptr) {
        char* spelled = gdk_rgba_to_string(colour);
        text = spelled;
        g_free(spelled);
    }
    if (colour != nullptr) {
        gdk_rgba_free(colour);
    }
    return text;
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

// The text that link_colour() gives for a colour written as "#rrggbb".
std::string colour_text(const char* hex) {
    GdkRGBA colour;
    EXPECT_TRUE(gdk_rgba_parse(&colour, hex));
    char* spelled = gdk_rgba_to_string(&colour);
    std::string text = spelled;
    g_free(spelled);
    return text;
}

// The link colour that palette_css(shift) gives, spelled as link_colour() spells it.
std::string link_colour_of(int shift) {
    char hex[16];
    g_snprintf(hex, sizeof hex, "#%02x%02x%02x", 16 + shift, 40, 200);
    GdkRGBA colour;
    EXPECT_TRUE(gdk_rgba_parse(&colour, hex));
    char* spelled = gdk_rgba_to_string(&colour);
    std::string text = spelled;
    g_free(spelled);
    return text;
}

// Turns GTK's dark flag round (the program's Style does it after it has loaded a palette) and
// returns the value it had.
bool flip_dark_flag() {
    GtkSettings* settings = gtk_settings_get_default();
    gboolean dark = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &dark, nullptr);
    g_object_set(settings, "gtk-application-prefer-dark-theme", dark == FALSE, nullptr);
    return dark != FALSE;
}

class HelpWindowTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        vermilion_manual_register_resource();
        vermilion_register_resource();
        g_display = gtk_init_check();
    }

    void SetUp() override {
        if (!g_display) {
            GTEST_SKIP() << "no display";
        }
        app_.reset(gtk_application_new("io.github.adamjeremy_sk.VermilionHelpWindowTest",
                                       G_APPLICATION_NON_UNIQUE));
        ASSERT_TRUE(g_application_register(G_APPLICATION(app_.get()), nullptr, nullptr));
        parent_ = GTK_WINDOW(gtk_window_new()); // toplevel; destroyed in TearDown
        library_.prepare();
        wait_ready(library_);
        ASSERT_TRUE(library_.ready());
        help_ = std::make_unique<HelpWindow>(app_.get(), parent_, library_);
    }

    void TearDown() override {
        if (help_ != nullptr) {
            GtkWindow* window = help_->window();
            help_.reset();
            gtk_window_destroy(window);
        }
        if (parent_ != nullptr) {
            gtk_window_destroy(parent_);
        }
    }

    const ManualText& page() { return help_->page_view().page(); }

    // True when the action "back" of the window can be used.
    bool back_enabled() const {
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(help_->window()), "back");
        return action != nullptr && g_action_get_enabled(action) != FALSE;
    }

    std::unique_ptr<GtkApplication, void (*)(gpointer)> app_{nullptr, &g_object_unref};
    GtkWindow* parent_ = nullptr;
    ManualLibrary library_; // before help_, so it is destroyed after it
    std::unique_ptr<HelpWindow> help_;
};

TEST_F(HelpWindowTest, ItOpensOnTheIndexPage) {
    EXPECT_EQ(page().text, page_of_file("index.md").text);
    EXPECT_FALSE(page().text.empty());
    EXPECT_FALSE(back_enabled());
    EXPECT_EQ(gtk_window_get_transient_for(help_->window()), parent_);
    EXPECT_EQ(std::string(gtk_window_get_title(help_->window())),
              std::string("Vermilion Editor Help"));
    EXPECT_TRUE(gtk_widget_has_css_class(GTK_WIDGET(help_->window()), "vm-help"));
    EXPECT_TRUE(gtk_widget_has_css_class(GTK_WIDGET(help_->window()), "vm-main"));
}

TEST_F(HelpWindowTest, AClickOnALinkOpensItsTopicAndBackComesBack) {
    ASSERT_FALSE(page().links.empty());
    const std::string target = page().links[0].target;
    help_->page_view().activate_at(page().links[0].begin);
    EXPECT_EQ(page().text, page_of_file(target).text);
    EXPECT_NE(page().text, page_of_file("index.md").text);
    EXPECT_TRUE(back_enabled());
    help_->back();
    EXPECT_EQ(page().text, page_of_file("index.md").text);
    EXPECT_FALSE(back_enabled());
}

TEST_F(HelpWindowTest, OpeningThePageThatIsOpenChangesNothing) {
    EXPECT_FALSE(back_enabled());
    help_->open_topic("index.md");
    EXPECT_FALSE(back_enabled());
}

TEST_F(HelpWindowTest, EveryTopicOfTheManualOpens) {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(VERMILION_MANUAL_DIR)) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".md") {
            continue;
        }
        SCOPED_TRACE(name);
        help_->open_topic(name);
        EXPECT_EQ(page().text, page_of_file(name).text);
        EXPECT_FALSE(page().text.empty());
        ++count;
    }
    EXPECT_GE(count, 10);
}

TEST_F(HelpWindowTest, BeforeTheManualIsReadyThePageSaysLoadingAndThenFillsIn) {
    ManualLibrary late; // not prepared
    HelpWindow early(app_.get(), parent_, late);
    EXPECT_EQ(early.page_view().page().text, "Loading...");
    EXPECT_FALSE(early.page_view().page().text.empty());
    late.prepare();
    wait_ready(late);
    EXPECT_EQ(early.page_view().page().text, page_of_file("index.md").text);
    gtk_window_destroy(early.window());
}

TEST_F(HelpWindowTest, ATopicOpenedWhileLoadingShowsItWhenTheManualIsReady) {
    ManualLibrary late; // not prepared
    HelpWindow early(app_.get(), parent_, late);
    early.open_topic("settings.md");
    EXPECT_EQ(early.page_view().page().text, "Loading...");
    late.prepare();
    wait_ready(late);
    EXPECT_EQ(early.page_view().page().text, page_of_file("settings.md").text);
    early.back();
    EXPECT_EQ(early.page_view().page().text, page_of_file("index.md").text);
    gtk_window_destroy(early.window());
}

TEST_F(HelpWindowTest, AWindowThatIsGoneBeforeTheManualIsReadyIsNeverCalled) {
    ManualLibrary late; // not prepared
    {
        HelpWindow early(app_.get(), parent_, late);
        gtk_window_destroy(early.window());
    }
    late.prepare();
    wait_ready(late);
    spin(50);
    EXPECT_TRUE(late.ready());
}

TEST_F(HelpWindowTest, ATopicThatIsNotInTheProgramSaysSo) {
    ASSERT_TRUE(vermilion::is_topic_name("no-such-topic.md"));
    help_->open_topic("no-such-topic.md");
    EXPECT_NE(page().text.find("This topic is not available: no-such-topic.md"), std::string::npos);
    EXPECT_TRUE(back_enabled());
}

TEST_F(HelpWindowTest, ANameThatIsNotATopicIsIgnored) {
    const std::string before = page().text;
    for (const char* name : {"", "../index.md", "http://example.org/index.md", "index", "a b.md"}) {
        SCOPED_TRACE(name);
        ASSERT_FALSE(vermilion::is_topic_name(name));
        help_->open_topic(name);
        EXPECT_EQ(page().text, before);
        EXPECT_FALSE(back_enabled());
    }
}

// The side pane: the notebook that holds the tree and the Search tab.
GtkWidget* pane_of(HelpWindow& help) {
    return gtk_widget_get_ancestor(help.contents().widget(), GTK_TYPE_NOTEBOOK);
}

TEST_F(HelpWindowTest, ItOpensThirtySixLinesHighAndAHundredColumnsWide) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    gtk_window_present(help_->window());
    spin(500);
    // The text area is exactly 36 lines and the margins above and below.
    EXPECT_EQ(static_cast<int>(gtk_adjustment_get_page_size(view.vadjustment())),
              view.height_for_lines(vermilion::ui::kHelpPageLines));
    EXPECT_EQ(vermilion::ui::kHelpPageLines, 36);
    const int wide = view.width_for_columns(vermilion::ui::kHelpOpenColumns);
    EXPECT_EQ(gtk_widget_get_width(view.widget()), wide);
    EXPECT_EQ(view.columns(), static_cast<std::size_t>(vermilion::ui::kHelpOpenColumns));
    EXPECT_EQ(vermilion::ui::kHelpOpenColumns, 100);
    // The pane is 28 columns of the page's font wide, and the window has room for it, the divider
    // and the page.
    GtkWidget* split = gtk_widget_get_parent(pane_of(*help_));
    const int pane = gtk_paned_get_position(GTK_PANED(split));
    EXPECT_EQ(pane, view.width_for_columns(vermilion::ui::kHelpPaneColumns));
    EXPECT_EQ(vermilion::ui::kHelpPaneColumns, 28);
    graphene_rect_t bounds;
    ASSERT_TRUE(gtk_widget_compute_bounds(view.widget(), split, &bounds));
    const int divider = static_cast<int>(bounds.origin.x) - pane;
    EXPECT_GE(divider, 0);
    EXPECT_LT(divider, 20);
    EXPECT_EQ(gtk_widget_get_width(GTK_WIDGET(help_->window())), pane + divider + wide);
}

TEST_F(HelpWindowTest, ThePaneIsAtTheLeftOfThePageInAPanedWidget) {
    GtkWidget* pane = pane_of(*help_);
    ASSERT_NE(pane, nullptr);
    GtkWidget* split = gtk_widget_get_parent(pane);
    ASSERT_TRUE(GTK_IS_PANED(split));
    EXPECT_EQ(gtk_paned_get_start_child(GTK_PANED(split)), pane);
    EXPECT_EQ(gtk_paned_get_end_child(GTK_PANED(split)), help_->page_view().widget());
    EXPECT_EQ(gtk_paned_get_position(GTK_PANED(split)),
              help_->page_view().width_for_columns(vermilion::ui::kHelpPaneColumns));
}

// The position in help_->contents().rows() of the topic row with this title, or -1.
int topic_row(HelpWindow& help, const std::string& title) {
    const auto rows = help.contents().rows();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows[i].heading && rows[i].title == title) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// The title of the topic row of `file` in the manual's outline ("" when it is not in it).
std::string title_of(const ManualLibrary& library, const std::string& file) {
    for (const vermilion::OutlineTopic& topic : library.data()->outline) {
        if (topic.file == file) {
            return topic.title;
        }
    }
    return "";
}

TEST_F(HelpWindowTest, TheTreeHasARowForEveryTopicAndSelectsThePageOnTheScreen) {
    const std::vector<vermilion::OutlineTopic>& outline = library_.data()->outline;
    ASSERT_GE(outline.size(), std::size_t{10});
    std::size_t topics = 0;
    for (const auto& row : help_->contents().rows()) {
        topics += row.heading ? 0 : 1;
    }
    EXPECT_EQ(topics, outline.size());
    EXPECT_FALSE(help_->contents().loading());
    EXPECT_EQ(help_->contents().current(), "index.md");
    ASSERT_TRUE(help_->contents().selected().has_value());
    EXPECT_EQ(help_->contents().rows()[*help_->contents().selected()].title,
              title_of(library_, "index.md"));
}

TEST_F(HelpWindowTest, TheTreeFollowsLinksBackAndTheOtherWindowCalls) {
    help_->open_topic("settings.md");
    EXPECT_EQ(help_->contents().current(), "settings.md");
    ASSERT_TRUE(help_->contents().selected().has_value());
    EXPECT_EQ(help_->contents().rows()[*help_->contents().selected()].title,
              title_of(library_, "settings.md"));
    help_->back();
    EXPECT_EQ(help_->contents().current(), "index.md");
    EXPECT_EQ(help_->contents().rows()[*help_->contents().selected()].title,
              title_of(library_, "index.md"));
}

TEST_F(HelpWindowTest, AClickOnATopicRowOpensItsPage) {
    const int row = topic_row(*help_, title_of(library_, "settings.md"));
    ASSERT_GE(row, 0);
    help_->contents().activate_row(static_cast<std::size_t>(row));
    EXPECT_EQ(page().text, page_of_file("settings.md").text);
    EXPECT_TRUE(back_enabled());
    EXPECT_EQ(help_->contents().current(), "settings.md");
}

TEST_F(HelpWindowTest, AClickOnAHeadingRowOpensItsPageAndScrollsToTheHeading) {
    // The topic with the most headings; its last heading is the lowest on the page.
    const vermilion::OutlineTopic* richest = nullptr;
    for (const vermilion::OutlineTopic& topic : library_.data()->outline) {
        if (richest == nullptr || topic.headings.size() > richest->headings.size()) {
            richest = &topic;
        }
    }
    ASSERT_NE(richest, nullptr);
    ASSERT_GE(richest->headings.size(), std::size_t{3});
    gtk_window_present(help_->window());
    spin(300);
    // The reader opens the topic in the tree (the page on the screen is still the index) and
    // clicks its last heading.
    ASSERT_TRUE(help_->contents().set_topic_open(richest->file, true));
    const std::size_t heading_row =
        static_cast<std::size_t>(topic_row(*help_, richest->title)) + richest->headings.size();
    ASSERT_LT(heading_row, help_->contents().rows().size());
    ASSERT_TRUE(help_->contents().rows()[heading_row].heading);
    EXPECT_EQ(page().text, page_of_file("index.md", help_->page_view().columns()).text);
    help_->contents().activate_row(heading_row);
    EXPECT_EQ(page().text, page_of_file(richest->file, help_->page_view().columns()).text);
    for (int i = 0; i < 100 && gtk_adjustment_get_value(help_->page_view().vadjustment()) < 1;
         ++i) {
        spin(50);
    }
    EXPECT_GT(gtk_adjustment_get_value(help_->page_view().vadjustment()), 1);
    // The row of the page is selected again, not the heading row.
    EXPECT_EQ(help_->contents().current(), richest->file);
    ASSERT_TRUE(help_->contents().selected().has_value());
    EXPECT_EQ(help_->contents().rows()[*help_->contents().selected()].title, richest->title);
    EXPECT_FALSE(help_->contents().rows()[*help_->contents().selected()].heading);
}

TEST_F(HelpWindowTest, WhileTheManualIsLoadingTheTreeSaysSoAndThenFillsIn) {
    ManualLibrary late; // not prepared
    HelpWindow early(app_.get(), parent_, late);
    EXPECT_TRUE(early.contents().loading());
    EXPECT_TRUE(early.contents().rows().empty());
    early.open_topic("settings.md");
    late.prepare();
    wait_ready(late);
    EXPECT_FALSE(early.contents().loading());
    EXPECT_EQ(early.contents().current(), "settings.md");
    ASSERT_TRUE(early.contents().selected().has_value());
    EXPECT_EQ(early.contents().rows()[*early.contents().selected()].title,
              title_of(late, "settings.md"));
    gtk_window_destroy(early.window());
}

TEST_F(HelpWindowTest, ThePageIsLaidOutAgainForTheWidthItHas) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    help_->open_topic("settings.md");
    gtk_window_present(help_->window());
    spin(500);
    EXPECT_EQ(page().text, page_of_file("settings.md", 100).text);
    ASSERT_TRUE(view.set_zoom(1.5));
    EXPECT_LT(view.columns(), std::size_t{100});
    EXPECT_EQ(page().text, page_of_file("settings.md", view.columns()).text);
    EXPECT_NE(page().text, page_of_file("settings.md", 100).text);
}

TEST_F(HelpWindowTest, TheOpeningWidthFollowsTheZoom) {
    vermilion::ui::HelpPageView& view = help_->page_view();
    const int before = view.width_for_columns(vermilion::ui::kHelpOpenColumns);
    ASSERT_TRUE(view.set_zoom(2.0));
    EXPECT_GT(view.width_for_columns(vermilion::ui::kHelpOpenColumns),
              before * 1.8); // twice the page, whatever the font is
}

TEST_F(HelpWindowTest, ThePageReadsItsColoursAgainWhenTheDarkFlagChanges) {
    PaletteOnDisplay palette(0);
    help_->page_view().refresh_colours();
    EXPECT_EQ(link_colour(*help_), link_colour_of(0));
    palette.load(5);
    EXPECT_EQ(link_colour(*help_), link_colour_of(0)); // nothing has told the page yet
    const bool was = flip_dark_flag();
    EXPECT_EQ(link_colour(*help_), link_colour_of(5));
    palette.load(9);
    flip_dark_flag();
    EXPECT_EQ(link_colour(*help_), link_colour_of(9));
    if (was != FALSE) {
        flip_dark_flag();
    }
}

// The program's own stylesheets: the dark palette gives the links their light blue.
TEST_F(HelpWindowTest, TheProgramsPaletteSwitchChangesTheColoursOfThePage) {
    GtkSettings* settings = gtk_settings_get_default();
    gboolean flag = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &flag, nullptr);
    {
        std::unique_ptr<vermilion::ui::Style> style = vermilion::ui::Style::install(false);
        ASSERT_NE(style, nullptr);
        help_->page_view().refresh_colours();
        EXPECT_EQ(link_colour(*help_), colour_text("#0a246a"));
        ASSERT_TRUE(style->set_dark(true));
        EXPECT_EQ(link_colour(*help_), colour_text("#7fb2ff"));
        ASSERT_TRUE(style->set_dark(false));
        EXPECT_EQ(link_colour(*help_), colour_text("#0a246a"));
    }
    g_object_set(settings, "gtk-application-prefer-dark-theme", flag, nullptr);
}

TEST_F(HelpWindowTest, AfterTheWindowIsGoneTheFlagMeansNothingToIt) {
    GtkWindow* window = help_->window();
    help_.reset();
    gtk_window_destroy(window);
    const bool was = flip_dark_flag();
    flip_dark_flag();
    if (was) {
        flip_dark_flag();
        flip_dark_flag();
    }
    SUCCEED();
}

// The search that the tests run: a word that many pages of the manual have.
vermilion::SearchOptions settings_search() {
    vermilion::SearchOptions options;
    options.text = "settings";
    return options;
}

TEST_F(HelpWindowTest, ThePaneHasAContentsTabAndASearchTab) {
    GtkNotebook* tabs = GTK_NOTEBOOK(pane_of(*help_));
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(gtk_notebook_get_n_pages(tabs), 2);
    EXPECT_STREQ(gtk_notebook_get_tab_label_text(tabs, gtk_notebook_get_nth_page(tabs, 0)),
                 "Contents");
    EXPECT_STREQ(gtk_notebook_get_tab_label_text(tabs, gtk_notebook_get_nth_page(tabs, 1)),
                 "Search");
    EXPECT_EQ(gtk_notebook_get_nth_page(tabs, 0), help_->contents().widget());
    EXPECT_EQ(gtk_notebook_get_nth_page(tabs, 1), help_->search().widget());
    EXPECT_FALSE(help_->search_tab_shown());
    EXPECT_EQ(help_->status_text(), "");
}

TEST_F(HelpWindowTest, ShowSearchSwitchesToTheSearchTabAndPutsTheKeyboardInTheBox) {
    gtk_window_present(help_->window());
    spin(300);
    help_->show_search();
    EXPECT_TRUE(help_->search_tab_shown());
    GtkWidget* focus = gtk_window_get_focus(help_->window()); // borrowed
    ASSERT_NE(focus, nullptr);
    EXPECT_TRUE(gtk_widget_is_ancestor(focus, help_->search().widget()));
}

TEST_F(HelpWindowTest, ASearchListsThePagesThatHaveAMatchAndSaysHowMany) {
    const vermilion::SearchOptions options = settings_search();
    help_->search().set_options(options);
    help_->search().run(); // what Enter in the box does
    const std::vector<vermilion::PageResult> expected = vermilion::search_pages(
        vermilion::ui::manual_search_pages(*library_.data(), help_->page_view().columns()),
        options);
    const std::vector<vermilion::PageResult>& shown = help_->search().results();
    ASSERT_GE(expected.size(), std::size_t{3});
    ASSERT_EQ(shown.size(), expected.size());
    for (std::size_t i = 0; i < shown.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(shown[i].file, expected[i].file);
        EXPECT_EQ(shown[i].count, expected[i].count);
        EXPECT_GT(shown[i].count, std::size_t{0});
    }
    EXPECT_EQ(help_->status_text(), vermilion::ui::results_message(expected));
    // The page on the screen (the index) shows the matches of the search.
    EXPECT_EQ(help_->page_view().search(), options);
    EXPECT_EQ(help_->page_view().matches().size(),
              vermilion::find_matches(page().text, options).size());
}

TEST_F(HelpWindowTest, AnEmptySearchClearsTheResultsAndTheHighlights) {
    help_->run_search(settings_search());
    ASSERT_FALSE(help_->search().results().empty());
    ASSERT_FALSE(help_->page_view().matches().empty());
    help_->run_search(vermilion::SearchOptions{});
    EXPECT_TRUE(help_->search().results().empty());
    EXPECT_TRUE(help_->page_view().matches().empty());
    EXPECT_EQ(help_->status_text(), "");
    help_->open_topic("settings.md"); // a page that opens afterwards has no highlights either
    EXPECT_TRUE(help_->page_view().matches().empty());
    EXPECT_EQ(help_->status_text(), "");
}

TEST_F(HelpWindowTest, ABadPatternShowsItsMessageAndFindsNothing) {
    help_->run_search(settings_search());
    ASSERT_FALSE(help_->search().results().empty());
    vermilion::SearchOptions bad;
    bad.text = "(";
    bad.regex = true;
    help_->run_search(bad);
    EXPECT_TRUE(help_->search().results().empty());
    EXPECT_TRUE(help_->page_view().matches().empty());
    EXPECT_EQ(help_->status_text().rfind("Not a valid pattern: ", 0), std::size_t{0});
    help_->open_topic("settings.md"); // the message stays while pages open
    EXPECT_EQ(help_->status_text().rfind("Not a valid pattern: ", 0), std::size_t{0});
}

TEST_F(HelpWindowTest, APageThatOpensShowsTheMatchesOfTheLastSearchAndCountsThem) {
    help_->run_search(settings_search());
    help_->open_topic("settings.md");
    const std::size_t count = help_->page_view().matches().size();
    ASSERT_GT(count, std::size_t{0});
    EXPECT_EQ(count, vermilion::find_matches(page().text, settings_search()).size());
    EXPECT_FALSE(help_->page_view().current_match().has_value());
    EXPECT_EQ(help_->status_text(), vermilion::match_count_message(0, static_cast<int>(count)));
}

TEST_F(HelpWindowTest, AClickOnAResultRowOpensItsPageAtTheFirstMatch) {
    help_->run_search(settings_search());
    const std::vector<vermilion::PageResult>& shown = help_->search().results();
    std::size_t row = shown.size();
    for (std::size_t i = 0; i < shown.size(); ++i) {
        if (shown[i].file != "index.md") {
            row = i;
            break;
        }
    }
    ASSERT_LT(row, shown.size());
    const std::string file = shown[row].file;
    const std::size_t count = shown[row].count;
    help_->search().activate_row(row);
    EXPECT_EQ(help_->contents().current(), file);
    EXPECT_EQ(page().text, page_of_file(file, help_->page_view().columns()).text);
    EXPECT_TRUE(back_enabled());
    ASSERT_TRUE(help_->page_view().current_match().has_value());
    EXPECT_EQ(*help_->page_view().current_match(), std::size_t{0});
    EXPECT_EQ(help_->page_view().matches().size(), count);
    EXPECT_EQ(help_->status_text(), vermilion::match_count_message(1, static_cast<int>(count)));
}

TEST_F(HelpWindowTest, NextAndPreviousStepThroughTheMatchesAndSayWhereTheyAre) {
    help_->run_search(settings_search());
    const std::vector<vermilion::PageResult>& shown = help_->search().results();
    std::size_t row = shown.size();
    for (std::size_t i = 0; i < shown.size(); ++i) {
        if (shown[i].count >= 3) {
            row = i;
            break;
        }
    }
    ASSERT_LT(row, shown.size()); // a page with at least three matches
    const int count = static_cast<int>(shown[row].count);
    help_->search().activate_row(row);
    ASSERT_EQ(help_->status_text(), vermilion::match_count_message(1, count));
    help_->search().step(true);
    EXPECT_EQ(help_->page_view().current_match(), std::optional<std::size_t>(1));
    EXPECT_EQ(help_->status_text(), vermilion::match_count_message(2, count));
    help_->search().step(false);
    EXPECT_EQ(help_->status_text(), vermilion::match_count_message(1, count));
    help_->search().step(false); // before the first match: around to the last
    EXPECT_EQ(help_->page_view().current_match(),
              std::optional<std::size_t>(static_cast<std::size_t>(count) - 1));
    EXPECT_EQ(help_->status_text(), vermilion::wrapped_message(false));
    help_->search().step(true); // after the last match: around to the first
    EXPECT_EQ(help_->page_view().current_match(), std::optional<std::size_t>(0));
    EXPECT_EQ(help_->status_text(), vermilion::wrapped_message(true));
}

TEST_F(HelpWindowTest, SteppingWithoutASearchChangesNothing) {
    help_->step_match(true);
    EXPECT_FALSE(help_->page_view().current_match().has_value());
    EXPECT_EQ(help_->status_text(), "");
}

TEST_F(HelpWindowTest, ASearchBeforeTheManualIsReadySaysSo) {
    ManualLibrary late; // not prepared
    HelpWindow early(app_.get(), parent_, late);
    early.run_search(settings_search());
    EXPECT_EQ(early.status_text(), "The manual is still loading.");
    EXPECT_TRUE(early.search().results().empty());
    EXPECT_FALSE(early.page_view().search().text == "settings");
    gtk_window_destroy(early.window());
}

} // namespace
