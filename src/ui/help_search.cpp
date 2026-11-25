// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Search tab of the Help window: the box, the options, Previous and Next and the results.

#include "ui/help_search.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include "core/count_text.h"
#include "core/search_messages.h"

namespace vermilion::ui {
namespace {

constexpr int kSpacing = 6;

// The number that a row's string holds: an index into the results.
std::size_t id_of(gpointer item) {
    return static_cast<std::size_t>(
        std::strtoull(gtk_string_object_get_string(GTK_STRING_OBJECT(item)), nullptr, 10));
}

// "1 match", "3 matches": the count and its noun.
std::string counted(std::uint64_t count, const char* one, const char* many) {
    return with_commas(count) + " " + (count == 1 ? one : many);
}

// Floating until a box takes it over.
GtkWidget* button(const char* label, const char* tooltip) {
    GtkWidget* result = gtk_button_new_with_label(label);
    gtk_widget_set_tooltip_text(result, tooltip);
    gtk_widget_set_hexpand(result, TRUE);
    return result;
}

} // namespace

std::string results_message(const std::vector<PageResult>& results) {
    if (results.empty()) {
        return match_count_message(0, 0); // "Not found"
    }
    std::uint64_t matches = 0;
    for (const PageResult& page : results) {
        matches += page.count;
    }
    return counted(matches, "match", "matches") + " on " + counted(results.size(), "page", "pages");
}

HelpSearch::HelpSearch(RunHandler on_run, OpenHandler on_open, StepHandler on_step)
    : on_run_(std::move(on_run)), on_open_(std::move(on_open)), on_step_(std::move(on_step)) {
    entry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_), "Search the manual");
    gtk_editable_set_width_chars(GTK_EDITABLE(entry_), 1); // the pane decides how wide it is
    gtk_widget_set_hexpand(entry_, TRUE);
    entry_signal_ = gtkx::connect<void(GObject*)>(entry_, "activate", [this](GObject*) { run(); });

    match_case_ = gtk_check_button_new_with_label("Match case");
    whole_word_ = gtk_check_button_new_with_label("Whole word");
    regex_ = gtk_check_button_new_with_label("Regex");

    GtkWidget* previous = button("Previous", "Previous match (Shift+F3)");
    GtkWidget* next = button("Next", "Next match (F3)");
    previous_signal_ =
        gtkx::connect<void(GObject*)>(previous, "clicked", [this](GObject*) { step(false); });
    next_signal_ = gtkx::connect<void(GObject*)>(next, "clicked", [this](GObject*) { step(true); });
    GtkWidget* steps = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kSpacing);
    gtk_box_append(GTK_BOX(steps), previous);
    gtk_box_append(GTK_BOX(steps), next);

    model_ = gtk_string_list_new(nullptr);
    GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(model_)); // takes the model
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new(); // the list view takes it
    setup_signal_ = gtkx::connect<void(GObject*, GObject*)>(
        factory, "setup", [this](GObject*, GObject* item) { setup_row(item); });
    bind_signal_ = gtkx::connect<void(GObject*, GObject*)>(
        factory, "bind", [this](GObject*, GObject* item) { bind_row(item); });
    GtkWidget* list = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory); // takes both
    gtk_widget_add_css_class(list, kHelpResultsClass);
    gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(list), TRUE);
    activate_signal_ = gtkx::connect<void(GObject*, guint)>(
        list, "activate", [this](GObject*, guint position) { activate_row(position); });

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list);
    gtk_widget_set_vexpand(scrolled, TRUE);

    root_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, kSpacing);
    gtk_widget_add_css_class(root_, kHelpSearchClass);
    gtk_widget_set_margin_start(root_, kSpacing);
    gtk_widget_set_margin_end(root_, kSpacing);
    gtk_widget_set_margin_top(root_, kSpacing);
    gtk_widget_set_margin_bottom(root_, kSpacing);
    gtk_box_append(GTK_BOX(root_), entry_);
    gtk_box_append(GTK_BOX(root_), match_case_);
    gtk_box_append(GTK_BOX(root_), whole_word_);
    gtk_box_append(GTK_BOX(root_), regex_);
    gtk_box_append(GTK_BOX(root_), steps);
    gtk_box_append(GTK_BOX(root_), scrolled);
}

HelpSearch::~HelpSearch() = default;

SearchOptions HelpSearch::options() const {
    SearchOptions options;
    options.text = gtk_editable_get_text(GTK_EDITABLE(entry_)); // borrowed
    options.match_case = gtk_check_button_get_active(GTK_CHECK_BUTTON(match_case_)) != FALSE;
    options.whole_word = gtk_check_button_get_active(GTK_CHECK_BUTTON(whole_word_)) != FALSE;
    options.regex = gtk_check_button_get_active(GTK_CHECK_BUTTON(regex_)) != FALSE;
    return options;
}

void HelpSearch::set_options(const SearchOptions& options) {
    gtk_editable_set_text(GTK_EDITABLE(entry_), options.text.c_str());
    gtk_check_button_set_active(GTK_CHECK_BUTTON(match_case_), options.match_case ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(whole_word_), options.whole_word ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(regex_), options.regex ? TRUE : FALSE);
}

void HelpSearch::focus_text() {
    gtk_widget_grab_focus(entry_);
    gtk_editable_select_region(GTK_EDITABLE(entry_), 0, -1);
}

void HelpSearch::run() {
    if (on_run_) {
        on_run_(options());
    }
}

void HelpSearch::step(bool forward) {
    if (on_step_) {
        on_step_(forward);
    }
}

void HelpSearch::set_results(const std::vector<PageResult>& results) {
    results_ = results;
    gtk_string_list_splice(model_, 0, g_list_model_get_n_items(G_LIST_MODEL(model_)), nullptr);
    for (std::size_t i = 0; i < results_.size(); ++i) {
        char text[32];
        std::snprintf(text, sizeof text, "%zu", i);
        gtk_string_list_append(model_, text);
    }
}

void HelpSearch::activate_row(std::size_t position) {
    if (position >= results_.size()) {
        return;
    }
    const std::string file = results_[position].file; // the handler may fill the list again
    if (on_open_) {
        on_open_(file);
    }
}

void HelpSearch::setup_row(GObject* item) {
    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    GtkWidget* count = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(count), 1.0f);
    GtkWidget* line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kSpacing);
    gtk_box_append(GTK_BOX(line), title);
    gtk_box_append(GTK_BOX(line), count);
    GtkWidget* snippet = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(snippet), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(snippet), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(snippet, kHelpSnippetClass);
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(row), line);
    gtk_box_append(GTK_BOX(row), snippet);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), row);
}

void HelpSearch::bind_row(GObject* item) {
    GtkListItem* list_item = GTK_LIST_ITEM(item);
    gpointer object = gtk_list_item_get_item(list_item); // borrowed
    GtkWidget* row = gtk_list_item_get_child(list_item); // borrowed
    if (object == nullptr || row == nullptr) {
        return;
    }
    const std::size_t id = id_of(object);
    if (id >= results_.size()) {
        return;
    }
    const PageResult& page = results_[id];
    GtkWidget* line = gtk_widget_get_first_child(row);
    GtkWidget* snippet = gtk_widget_get_next_sibling(line);
    GtkWidget* title = gtk_widget_get_first_child(line);
    GtkWidget* count = gtk_widget_get_next_sibling(title);
    gtk_label_set_text(GTK_LABEL(title), page.title.c_str());
    gtk_label_set_text(GTK_LABEL(count), with_commas(page.count).c_str());
    gtk_label_set_text(GTK_LABEL(snippet), page.snippet.c_str());
    gtk_widget_set_tooltip_text(row,
                                page.snippet.empty() ? page.title.c_str() : page.snippet.c_str());
}

} // namespace vermilion::ui
