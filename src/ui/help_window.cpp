// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the help window: the Back and Forward buttons, the side pane (the Contents tree and
// the Search tab), the page view that shows one page of the manual and the status line.

#include "ui/help_window.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/manual_page.h"
#include "core/manual_spans.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {
namespace {

constexpr const char* kIndexTopic = "index.md";

// A page of one paragraph with `text`.
ManualPage note_page(const std::string& text) {
    ManualPage page;
    ManualBlock block;
    block.spans.resize(1);
    block.spans[0].kind = SpanKind::Text;
    block.spans[0].text = text;
    page.blocks.push_back(std::move(block));
    return page;
}

// Floating until the bar takes it over.
GtkWidget* action_button(const char* label, const char* action, const char* tooltip) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action);
    gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

// The bar with the Back and Forward buttons, in that order; floating until the root box takes it
// over.
GtkWidget* build_bar() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(bar, "vm-help-bar");
    gtk_box_append(GTK_BOX(bar), action_button("Back", "win.back", "Back (Alt+Left)"));
    gtk_box_append(GTK_BOX(bar), action_button("Forward", "win.forward", "Forward (Alt+Right)"));
    return bar;
}

// The side pane: a notebook with the Contents tree and the Search tab. Floating until the paned
// widget takes it over.
GtkWidget* build_tabs(GtkWidget* contents, GtkWidget* search) {
    GtkWidget* tabs = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), contents, gtk_label_new("Contents"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), search, gtk_label_new("Search"));
    return tabs;
}

// The pane at the left of the page, with a divider that the reader can drag; when the pane is
// hidden the page has all the room. Floating until the root box takes it over.
GtkWidget* build_split(GtkWidget* pane, GtkWidget* page) {
    GtkWidget* split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(split), pane);
    gtk_paned_set_end_child(GTK_PANED(split), page);
    gtk_paned_set_resize_start_child(GTK_PANED(split), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(split), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(split), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(split), FALSE);
    gtk_widget_set_vexpand(split, TRUE);
    return split;
}

// The status line: a bar with one label (its first child), drawn like the status bar of the main
// window. Floating until the root box takes it over.
GtkWidget* build_status() {
    GtkWidget* text = gtk_label_new("");
    gtk_widget_add_css_class(text, "vm-status-message");
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(text, TRUE);
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(bar, "vm-statusbar");
    gtk_box_append(GTK_BOX(bar), text);
    return bar;
}

// The natural height of `widget`, in pixels.
int natural_height(GtkWidget* widget) {
    int natural = 0;
    gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL, -1, nullptr, &natural, nullptr, nullptr);
    return natural;
}

// The natural width of `widget`, in pixels.
int natural_width(GtkWidget* widget) {
    int natural = 0;
    gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, nullptr, &natural, nullptr, nullptr);
    return natural;
}

} // namespace

HelpWindow::HelpWindow(GtkApplication* app, GtkWindow* parent, ManualLibrary& library)
    : window_(GTK_WINDOW(gtk_application_window_new(app))), // GTK owns the window; it is borrowed
      library_(library) {
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window_), FALSE);
    gtk_window_set_title(window_, "Vermilion Editor Help");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-help");
    add_actions(app);
    create_parts();
    assemble();
    follow_palette();
    follow_library();
    open_topic(kIndexTopic);
}

void HelpWindow::create_parts() {
    page_view_ =
        std::make_unique<HelpPageView>([this](const std::string& target) { open_topic(target); });
    contents_ =
        std::make_unique<HelpContents>([this](const std::string& file) { open_topic(file); },
                                       [this](const std::string& file, std::size_t index) {
                                           open_topic(file);
                                           page_view_->scroll_to_heading(index);
                                       });
    search_ =
        std::make_unique<HelpSearch>([this](const SearchOptions& options) { run_search(options); },
                                     [this](const std::string& file) { open_result(file); },
                                     [this](bool forward) { step_match(forward); });
}

void HelpWindow::assemble() {
    GtkWidget* bar = build_bar(); // floating; the root box takes it over
    tabs_ = build_tabs(contents_->widget(), search_->widget()); // floating until the split has it
    GtkWidget* page = page_view_->widget(); // borrowed; the view owns it until the split has it
    GtkWidget* split = build_split(tabs_, page); // floating; the root box takes it over
    GtkWidget* statusbar = build_status();       // floating; the root box takes it over
    status_ = gtk_widget_get_first_child(statusbar);
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); // floating; the window takes it
    gtk_box_append(GTK_BOX(root), bar);
    gtk_box_append(GTK_BOX(root), split);
    gtk_box_append(GTK_BOX(root), statusbar);
    gtk_window_set_child(window_, root);
    // The pane is kHelpPaneColumns columns of the page's font wide. The window opens wide enough
    // for that, the divider (what the paned widget needs beyond its two children) and
    // kHelpOpenColumns columns of text, and kHelpPageLines lines high between the bar and the
    // status line.
    const int pane_width = page_view_->width_for_columns(kHelpPaneColumns);
    gtk_widget_set_size_request(tabs_, pane_width, -1);
    gtk_paned_set_position(GTK_PANED(split), pane_width);
    const int divider = natural_width(split) - natural_width(tabs_) - natural_width(page);
    gtk_window_set_default_size(
        window_, pane_width + divider + page_view_->width_for_columns(kHelpOpenColumns),
        natural_height(bar) + page_view_->height_for_lines(kHelpPageLines) +
            natural_height(statusbar));
}

void HelpWindow::follow_palette() {
    GtkSettings* settings = gtk_settings_get_default(); // borrowed; null without a display
    if (settings != nullptr) {
        palette_signal_ = gtkx::connect<void(GObject*, GParamSpec*)>(
            settings, "notify::gtk-application-prefer-dark-theme",
            [this](GObject*, GParamSpec*) { page_view_->refresh_colours(); });
    }
}

void HelpWindow::follow_library() {
    if (library_.ready()) {
        contents_->set_outline(library_.data()->outline);
    } else {
        wait_ = library_.on_ready([this] {
            contents_->set_outline(library_.data()->outline);
            show(history_.current());
        });
    }
}

void HelpWindow::present() {
    gtk_window_present(window_);
}

void HelpWindow::back() {
    std::optional<std::string> page = history_.back();
    if (page.has_value()) {
        show(*page);
        update_actions();
    }
}

void HelpWindow::forward() {
    std::optional<std::string> page = history_.forward();
    if (page.has_value()) {
        show(*page);
        update_actions();
    }
}

void HelpWindow::open_topic(const std::string& name) {
    if (is_topic_name(name) && history_.open(name)) {
        show(name);
        update_actions();
    }
}

void HelpWindow::show(const std::string& name) {
    contents_->set_current(name);
    const ManualData* manual = library_.data();
    if (manual == nullptr) {
        page_view_->show_source(note_page("Loading..."));
        return;
    }
    const auto found = manual->pages.find(name);
    if (found != manual->pages.end()) {
        page_view_->show_source(found->second);
    } else {
        page_view_->show_source(note_page("This topic is not available: " + name));
    }
    update_match_status();
}

} // namespace vermilion::ui
