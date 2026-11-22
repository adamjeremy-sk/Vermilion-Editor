// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Help page: a read-only text view that shows one ManualText page in exactly kManualColumns
// columns, with its links and its headings.

#pragma once

#include <gtk/gtk.h>

#include <cstddef>
#include <functional>
#include <string>

#include "core/manual_text.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// CSS classes: the text view, and the inner scrolled window around it. The stylesheet gives the
// view its font (the bundled font at 12 pt).
inline constexpr char kHelpViewClass[] = "vm-help-view";
inline constexpr char kHelpHolderClass[] = "vm-help-holder";

// The names of the event controllers of the view (gtk_event_controller_get_name), for tests.
inline constexpr char kHelpClickController[] = "help-page-click";
inline constexpr char kHelpMotionController[] = "help-page-motion";

// The names of the text tags, for look-ups in the view's tag table (tests, and later steps).
inline constexpr char kTagBody[] = "body"; // the whole page
inline constexpr char kTagItem[] = "item"; // list items: hanging indent of two columns
inline constexpr char kTagTableHeader[] = "table-header"; // bold
inline constexpr char kTagBold[] = "bold";                // bold
inline constexpr char kTagCode[] = "code";                // no property yet
inline constexpr char kTagLink[] = "link";                // underlined
inline constexpr char kTagHeading1[] = "h1";              // bold, 1.5 times the body text
inline constexpr char kTagHeading2[] = "h2";              // bold, 1.25 times the body text
inline constexpr char kTagHeading3[] = "h3"; // bold, the size of the body text; levels 3 to 6

// Lines of text that the window shows when it opens (Adam, 2026-11-21).
inline constexpr int kHelpPageLines = 36;
// Blank space above the first and below the last line, in pixels.
inline constexpr int kHelpPageMargin = 8;

// One Help page on the screen. Main loop only.
//
// Layout. The page is as wide as kManualColumns characters of the view's font (page_width()),
// centred in the space the window gives it. When that space is narrower than the page, the page
// keeps its width and the window scrolls sideways; the text never re-wraps to another width, so
// the tables built by core/manual_table.h stay whole. A line that is longer than the page wraps
// after exactly kManualColumns characters (headings, bigger than the body text, wrap sooner).
// Everything is selectable and copyable; nothing can be edited.
//
// A GtkTextView that is the direct child of a GtkScrolledWindow follows the window's width, so the
// widget has two scrolled windows: the outer one scrolls sideways (never up or down) and holds the
// inner one, which is page_width() wide and centred; the inner one holds the view and scrolls it up
// and down (its own bar is not drawn: a GtkScrollbar sits at the right edge of the widget, so the
// bar stays in view when the page is wider than the window).
class HelpPageView {
public:
    // Called with the topic file of a link ("open-and-save.md") when the reader clicks it.
    using LinkHandler = std::function<void(const std::string& target)>;

    // Builds the widgets, empty. `on_link` may be empty.
    explicit HelpPageView(LinkHandler on_link);

    HelpPageView(const HelpPageView&) = delete;
    HelpPageView& operator=(const HelpPageView&) = delete;

    // The widget to put into a window (a box with the page and its vertical bar); floating until
    // the caller puts it into a container. The view shares the life of the widget: keep both for
    // as long as the page is shown, destroy this object when the window that holds the widget is
    // destroyed.
    GtkWidget* widget() const { return root_; }
    // The text view inside it (borrowed).
    GtkTextView* text_view() const { return view_; }
    // The adjustments that scroll the page up and down (the inner window's) and sideways (the
    // outer window's); borrowed.
    GtkAdjustment* vadjustment() const;
    GtkAdjustment* hadjustment() const;

    // Shows `page`: replaces the text, applies the tags, drops the selection and scrolls to the
    // top. Link targets and offsets are those of `page`.
    void show(const ManualText& page);
    // The page on the screen (empty text before the first show()).
    const ManualText& page() const { return page_; }

    // What a click at character `offset` does: when a link of the page covers it, calls the link
    // handler with the target and returns true; false otherwise. A click on the view calls this
    // unless text is selected.
    bool activate_at(std::size_t offset);
    // The pointer is over character `offset`: the cursor is a pointer over a link and a text
    // cursor elsewhere. A pointer that leaves the view calls this with page().length (past the
    // last character: never a link).
    void hover_at(std::size_t offset);
    // True when the last hover_at() was over a link.
    bool pointer_on_link() const { return pointer_on_link_; }

    // The width of the page in pixels: 101.5 advances of the digit "0" in the view's font,
    // rounded. GTK wraps a line of this width after exactly kManualColumns characters (it wants
    // one character of room beyond the last column; the half is slack for rounding).
    int page_width() const;
    // The height of one line of body text, in pixels.
    int line_height() const;
    // The height of `lines` lines of body text plus the margins above and below, in pixels.
    int height_for_lines(int lines) const;

private:
    void build_tags();
    // Sets what depends on the font in use: the hanging indent of list items and the width of
    // the page.
    void apply_font();
    void apply_page_tags();
    void on_click(double x, double y);
    void on_motion(double x, double y);
    // The character under the point (x, y) of the view, or page().length.
    std::size_t offset_at(double x, double y) const;

    LinkHandler on_link_;
    ManualText page_;
    bool pointer_on_link_ = false;

    GtkWidget* root_ = nullptr;   // borrowed; floating, then owned by its container
    GtkWidget* outer_ = nullptr;  // borrowed; the root box owns it: scrolls sideways
    GtkWidget* inner_ = nullptr;  // borrowed; the outer window owns it: page_width() wide
    GtkTextView* view_ = nullptr; // borrowed; the inner window owns it
    GtkTextBuffer* buffer_ = nullptr;

    // Borrowed from the buffer's tag table.
    GtkTextTag* body_ = nullptr;
    GtkTextTag* item_ = nullptr;
    GtkTextTag* table_header_ = nullptr;
    GtkTextTag* bold_ = nullptr;
    GtkTextTag* code_ = nullptr;
    GtkTextTag* link_ = nullptr;
    GtkTextTag* heading_[3] = {nullptr, nullptr, nullptr};

    gtkx::SignalConnection click_signal_;
    gtkx::SignalConnection motion_signal_;
    gtkx::SignalConnection leave_signal_;
    gtkx::SignalConnection map_signal_;
};

} // namespace vermilion::ui
