// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Contents tree of the Help window: a list view over the topics and their headings.

#include "ui/help_contents.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace vermilion::ui {
namespace {

constexpr const char* kLoadingPage = "loading";
constexpr const char* kTreePage = "tree";

// How far a heading row is moved in for every level below 2, in pixels.
constexpr int kIndentPixels = 12;

// The number that a row's string object holds.
std::size_t id_of(gpointer item) {
    return static_cast<std::size_t>(
        std::strtoull(gtk_string_object_get_string(GTK_STRING_OBJECT(item)), nullptr, 10));
}

// A string object that holds `id`, for a model (a new reference).
GtkStringObject* object_of(std::size_t id) {
    char text[32];
    std::snprintf(text, sizeof text, "%zu", id);
    return gtk_string_object_new(text);
}

} // namespace

HelpContents::HelpContents(TopicHandler on_topic, HeadingHandler on_heading)
    : on_topic_(std::move(on_topic)), on_heading_(std::move(on_heading)) {
    GtkWidget* loading = gtk_label_new("Loading...");
    gtk_widget_add_css_class(loading, kHelpLoadingClass);
    gtk_label_set_xalign(GTK_LABEL(loading), 0.0f);
    gtk_label_set_yalign(GTK_LABEL(loading), 0.0f);
    gtk_widget_set_hexpand(loading, TRUE);
    gtk_widget_set_vexpand(loading, TRUE);

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new(); // the list view takes it
    setup_signal_ = gtkx::connect<void(GObject*, GObject*)>(
        factory, "setup", [this](GObject*, GObject* item) { setup_row(item); });
    bind_signal_ = gtkx::connect<void(GObject*, GObject*)>(
        factory, "bind", [this](GObject*, GObject* item) { bind_row(item); });
    list_ = gtk_list_view_new(nullptr, factory);
    gtk_widget_add_css_class(list_, kHelpContentsClass);
    gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(list_), TRUE);
    activate_signal_ = gtkx::connect<void(GObject*, guint)>(
        list_, "activate", [this](GObject*, guint position) { activate_row(position); });

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list_);
    gtk_widget_set_vexpand(scrolled, TRUE);

    root_ = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(root_), loading, kLoadingPage);
    gtk_stack_add_named(GTK_STACK(root_), scrolled, kTreePage);
    gtk_stack_set_visible_child_name(GTK_STACK(root_), kLoadingPage);
}

HelpContents::~HelpContents() = default;

bool HelpContents::loading() const {
    const char* name = gtk_stack_get_visible_child_name(GTK_STACK(root_)); // borrowed
    return name != nullptr && std::string(name) == kLoadingPage;
}

void HelpContents::set_outline(const std::vector<OutlineTopic>& outline) {
    outline_ = outline;
    opened_by_tree_.reset();
    build_model();
    gtk_stack_set_visible_child_name(GTK_STACK(root_), kTreePage);
    apply_current();
}

void HelpContents::set_current(const std::string& file) {
    current_ = file;
    apply_current();
}

std::vector<HelpContents::Row> HelpContents::rows() const {
    std::vector<Row> result;
    if (tree_ == nullptr) {
        return result;
    }
    const unsigned int count = g_list_model_get_n_items(G_LIST_MODEL(tree_));
    for (unsigned int position = 0; position < count; ++position) {
        const std::optional<Entry> entry = entry_at(position);
        GtkTreeListRow* row = row_at(position); // a new reference
        if (entry.has_value() && row != nullptr) {
            Row line;
            line.title = title_of(*entry);
            line.heading = entry->heading.has_value();
            line.open = gtk_tree_list_row_get_expanded(row) != FALSE;
            result.push_back(std::move(line));
        }
        if (row != nullptr) {
            g_object_unref(row);
        }
    }
    return result;
}

std::optional<std::size_t> HelpContents::selected() const {
    if (selection_ == nullptr) {
        return std::nullopt;
    }
    const unsigned int position = gtk_single_selection_get_selected(selection_);
    if (position == GTK_INVALID_LIST_POSITION) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(position);
}

void HelpContents::activate_row(std::size_t position) {
    if (tree_ == nullptr) {
        return;
    }
    const std::optional<Entry> entry = entry_at(static_cast<unsigned int>(position));
    if (!entry.has_value()) {
        return;
    }
    const OutlineTopic& topic = outline_[entry->topic];
    const std::string file = topic.file; // the handler may change the tree
    if (entry->heading.has_value()) {
        const std::size_t index = topic.headings[*entry->heading].index;
        if (on_heading_) {
            on_heading_(file, index);
        }
    } else if (on_topic_) {
        on_topic_(file);
    }
    apply_current();
}

bool HelpContents::set_topic_open(const std::string& file, bool open) {
    if (tree_ == nullptr) {
        return false;
    }
    const std::optional<std::size_t> topic = topic_of(file);
    const std::optional<unsigned int> position =
        topic.has_value() ? topic_position(*topic) : std::nullopt;
    GtkTreeListRow* row = position.has_value() ? row_at(*position) : nullptr; // new reference
    const bool done = row != nullptr && gtk_tree_list_row_is_expandable(row) != FALSE;
    if (done) {
        gtk_tree_list_row_set_expanded(row, open ? TRUE : FALSE);
    }
    if (row != nullptr) {
        g_object_unref(row);
    }
    return done;
}

void HelpContents::build_model() {
    entries_.clear();
    GListStore* roots = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    for (std::size_t topic = 0; topic < outline_.size(); ++topic) {
        GtkStringObject* item = object_of(entries_.size());
        g_list_store_append(roots, item);
        g_object_unref(item);
        entries_.push_back(Entry{topic, std::nullopt});
        for (std::size_t heading = 0; heading < outline_[topic].headings.size(); ++heading) {
            entries_.push_back(Entry{topic, heading});
        }
    }
    GtkTreeListModel* tree = gtk_tree_list_model_new(G_LIST_MODEL(roots), FALSE, FALSE,
                                                     &HelpContents::children_of, this, nullptr);
    GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(tree)); // takes the tree
    gtk_single_selection_set_autoselect(selection, FALSE);
    gtk_single_selection_set_can_unselect(selection, TRUE);
    gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
    tree_ = tree;
    selection_ = selection;
    gtk_list_view_set_model(GTK_LIST_VIEW(list_), GTK_SELECTION_MODEL(selection)); // its own ref
    g_object_unref(selection);
}

GListModel* HelpContents::children_of(gpointer item, gpointer self) {
    const HelpContents* tree = static_cast<const HelpContents*>(self);
    const std::size_t id = id_of(item);
    if (id >= tree->entries_.size() || tree->entries_[id].heading.has_value()) {
        return nullptr;
    }
    const std::size_t topic = tree->entries_[id].topic;
    const std::size_t count = tree->outline_[topic].headings.size();
    if (count == 0) {
        return nullptr;
    }
    GListStore* store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    for (std::size_t heading = 0; heading < count; ++heading) {
        GtkStringObject* child = object_of(id + 1 + heading);
        g_list_store_append(store, child);
        g_object_unref(child);
    }
    return G_LIST_MODEL(store);
}

std::optional<std::size_t> HelpContents::topic_of(const std::string& file) const {
    for (std::size_t topic = 0; topic < outline_.size(); ++topic) {
        if (outline_[topic].file == file) {
            return topic;
        }
    }
    return std::nullopt;
}

const std::string& HelpContents::title_of(const Entry& entry) const {
    const OutlineTopic& topic = outline_[entry.topic];
    return entry.heading.has_value() ? topic.headings[*entry.heading].title : topic.title;
}

GtkTreeListRow* HelpContents::row_at(unsigned int position) const {
    return tree_ == nullptr ? nullptr : gtk_tree_list_model_get_row(tree_, position);
}

std::optional<HelpContents::Entry> HelpContents::entry_at(unsigned int position) const {
    GtkTreeListRow* row = row_at(position); // a new reference
    if (row == nullptr) {
        return std::nullopt;
    }
    gpointer item = gtk_tree_list_row_get_item(row); // a new reference
    std::optional<Entry> entry;
    if (item != nullptr) {
        const std::size_t id = id_of(item);
        if (id < entries_.size()) {
            entry = entries_[id];
        }
        g_object_unref(item);
    }
    g_object_unref(row);
    return entry;
}

std::optional<unsigned int> HelpContents::topic_position(std::size_t topic) const {
    if (tree_ == nullptr) {
        return std::nullopt;
    }
    const unsigned int count = g_list_model_get_n_items(G_LIST_MODEL(tree_));
    for (unsigned int position = 0; position < count; ++position) {
        const std::optional<Entry> entry = entry_at(position);
        if (entry.has_value() && !entry->heading.has_value() && entry->topic == topic) {
            return position;
        }
    }
    return std::nullopt;
}

bool HelpContents::topic_is_open(std::size_t topic) const {
    const std::optional<unsigned int> position = topic_position(topic);
    GtkTreeListRow* row = position.has_value() ? row_at(*position) : nullptr; // new reference
    const bool open = row != nullptr && gtk_tree_list_row_get_expanded(row) != FALSE;
    if (row != nullptr) {
        g_object_unref(row);
    }
    return open;
}

void HelpContents::apply_current() {
    if (tree_ == nullptr) {
        return;
    }
    const std::optional<std::size_t> current = topic_of(current_);
    if (!current.has_value()) {
        gtk_single_selection_set_selected(selection_, GTK_INVALID_LIST_POSITION);
        return;
    }
    if (opened_by_tree_ != current) {
        if (opened_by_tree_.has_value()) {
            set_topic_open(outline_[*opened_by_tree_].file, false);
            opened_by_tree_.reset();
        }
        if (!topic_is_open(*current)) {
            set_topic_open(outline_[*current].file, true);
            if (topic_is_open(*current)) {
                opened_by_tree_ = current;
            }
        }
    }
    const std::optional<unsigned int> position = topic_position(*current);
    if (position.has_value()) {
        gtk_single_selection_set_selected(selection_, *position);
        gtk_list_view_scroll_to(GTK_LIST_VIEW(list_), *position, GTK_LIST_SCROLL_NONE, nullptr);
    }
}

void HelpContents::setup_row(GObject* item) {
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    GtkWidget* expander = gtk_tree_expander_new();
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), label);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), expander);
}

void HelpContents::bind_row(GObject* item) {
    GtkListItem* list_item = GTK_LIST_ITEM(item);
    GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(list_item));        // borrowed
    GtkTreeExpander* expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item)); // borrowed
    GtkWidget* label = gtk_tree_expander_get_child(expander);                          // borrowed
    gtk_tree_expander_set_list_row(expander, row);
    gpointer object = gtk_tree_list_row_get_item(row); // a new reference
    if (object != nullptr) {
        const std::size_t id = id_of(object);
        if (id < entries_.size()) {
            const Entry& entry = entries_[id];
            gtk_label_set_text(GTK_LABEL(label), title_of(entry).c_str());
            gtk_widget_set_tooltip_text(label, title_of(entry).c_str()); // the pane is narrow
            int indent = 0;
            if (entry.heading.has_value()) {
                const int level = outline_[entry.topic].headings[*entry.heading].level;
                indent = kIndentPixels * std::max(0, level - 2);
            }
            gtk_widget_set_margin_start(label, indent);
        }
        g_object_unref(object);
    }
}

} // namespace vermilion::ui
