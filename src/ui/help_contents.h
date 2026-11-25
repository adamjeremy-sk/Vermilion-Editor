// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Contents tree of the Help window: the topics of the manual, each with its headings.

#pragma once

#include <gtk/gtk.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/manual_outline.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// CSS classes of the list and of the "Loading..." text.
inline constexpr char kHelpContentsClass[] = "vm-help-contents";
inline constexpr char kHelpLoadingClass[] = "vm-help-loading";

// Columns of the page's font that the pane is wide at the start.
inline constexpr int kHelpPaneColumns = 28;

// The Contents tree: a row for every topic of the manual, in the order of the index page, and
// under it the headings of its page. Main loop only.
//
// Until set_outline() has been called the widget says "Loading...". The row of the current page
// (set_current()) is selected and in view, and its topic is open; the other topics are closed,
// except those that the reader opened by hand: moving to another page opens that page's topic and
// closes only the topic that the tree itself opened, so a topic that the reader opened stays open.
// A click (or Enter) on a row calls a handler: on_topic with the file of a topic row, on_heading
// with the file and the heading's index (OutlineHeading::index) of a heading row. The handler
// shows the page and calls set_current(); the selection is on the row of the current page again
// after every click, also one on a heading row.
class HelpContents {
public:
    using TopicHandler = std::function<void(const std::string& file)>;
    using HeadingHandler = std::function<void(const std::string& file, std::size_t index)>;

    // Builds the widget, showing "Loading...". Either handler may be empty.
    HelpContents(TopicHandler on_topic, HeadingHandler on_heading);

    ~HelpContents();

    HelpContents(const HelpContents&) = delete;
    HelpContents& operator=(const HelpContents&) = delete;

    // The widget to put into a window; floating until then.
    GtkWidget* widget() const { return root_; }

    // True until set_outline() has been called.
    bool loading() const;

    // Shows the tree of `outline` (copied), every topic closed, then the current page as
    // set_current() does. Calling it again replaces the tree.
    void set_outline(const std::vector<OutlineTopic>& outline);

    // Makes `file` the current page (see above). A file that is not a topic of the tree leaves
    // nothing selected; the name is kept for a tree that arrives later.
    void set_current(const std::string& file);
    const std::string& current() const { return current_; }

    // One row on the screen.
    struct Row {
        std::string title;
        bool heading = false; // a heading row, one level below its topic's row
        bool open = false;    // a topic row whose headings are shown
    };
    // The rows on the screen, top to bottom.
    std::vector<Row> rows() const;
    // The position in rows() of the selected row, or nothing.
    std::optional<std::size_t> selected() const;

    // What a click on the row at `position` of rows() does; nothing for a position past the end.
    void activate_row(std::size_t position);

    // Opens or closes the topic `file` as the arrow of its row does. False when there is no such
    // topic or it has no headings.
    bool set_topic_open(const std::string& file, bool open);

private:
    // A row: a topic, or (heading set) one of its headings, an index into OutlineTopic::headings.
    struct Entry {
        std::size_t topic = 0;
        std::optional<std::size_t> heading;
    };

    void build_model();
    // The place of the topic file `file` in the outline.
    std::optional<std::size_t> topic_of(const std::string& file) const;
    // The text of the row `entry`.
    const std::string& title_of(const Entry& entry) const;
    // The entry of the row at `position` of the tree, or nothing.
    std::optional<Entry> entry_at(unsigned int position) const;
    // The position of the row of topic `topic`, or nothing.
    std::optional<unsigned int> topic_position(std::size_t topic) const;
    // The GtkTreeListRow at `position` (a new reference), or null.
    GtkTreeListRow* row_at(unsigned int position) const;
    bool topic_is_open(std::size_t topic) const;
    void apply_current();
    // The headings of a topic row as a model, for GtkTreeListModel.
    static GListModel* children_of(gpointer item, gpointer self);
    void setup_row(GObject* item);
    void bind_row(GObject* item);

    TopicHandler on_topic_;
    HeadingHandler on_heading_;
    std::vector<OutlineTopic> outline_;
    // A row of the list model holds a string "n", an index into entries_ (a topic's headings follow
    // its own entry).
    std::vector<Entry> entries_;
    std::string current_;
    // The topic that this object opened (and so closes when the page moves).
    std::optional<std::size_t> opened_by_tree_;

    GtkWidget* root_ = nullptr;               // the stack; floating until the caller takes it
    GtkWidget* list_ = nullptr;               // borrowed: the list view, in the stack
    GtkTreeListModel* tree_ = nullptr;        // borrowed: owned by selection_
    GtkSingleSelection* selection_ = nullptr; // borrowed: owned by the list view
    gtkx::SignalConnection setup_signal_;
    gtkx::SignalConnection bind_signal_;
    gtkx::SignalConnection activate_signal_;
};

} // namespace vermilion::ui
