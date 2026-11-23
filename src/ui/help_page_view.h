// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The Help page: a read-only text view that shows one ManualText page in exactly kManualColumns
// columns, with its links, headings, zoom and colours.

#pragma once

#include <gtk/gtk.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "core/manual_text.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// CSS classes: the text view, and the inner scrolled window around it. The stylesheet gives the
// view its font (the bundled font at 12 pt).
inline constexpr char kHelpViewClass[] = "vm-help-view";
inline constexpr char kHelpHolderClass[] = "vm-help-holder";

// The names of the event controllers of the widget (gtk_event_controller_get_name), for tests.
inline constexpr char kHelpClickController[] = "help-page-click";
inline constexpr char kHelpMotionController[] = "help-page-motion";
inline constexpr char kHelpScrollController[] = "help-page-scroll";

// The names of the text tags, for look-ups in the view's tag table (tests, and the colours of the
// next step).
inline constexpr char kTagBody[] = "body"; // the whole page: the zoom
inline constexpr char kTagItem[] = "item"; // list items: hanging indent of two columns
inline constexpr char kTagTableHeader[] = "table-header";
inline constexpr char kTagBold[] = "bold";
inline constexpr char kTagCode[] = "code";
inline constexpr char kTagLink[] = "link";   // underlined
inline constexpr char kTagHeading1[] = "h1"; // bold, 1.5 times the zoom
inline constexpr char kTagHeading2[] = "h2"; // bold, 1.25 times the zoom
inline constexpr char kTagHeading3[] = "h3"; // bold; levels 3 to 6

// Lines of text that the window shows when it opens (Adam, 2026-11-21).
inline constexpr int kHelpPageLines = 36;
// Blank space above the first and below the last line, in pixels (the text view's own top and
// bottom margins, so that a heading scrolled to the top is not clipped).
inline constexpr int kHelpPageMargin = 8;

// The colour names of the Help page, defined with @define-color in classic_light.css and
// classic_dark.css (the roles are in those files). refresh_colours() reads them.
inline constexpr char kColourH1[] = "vm_help_h1";                    // text of a level 1 heading
inline constexpr char kColourH2[] = "vm_help_h2";                    // text of levels 2 and 3
inline constexpr char kColourLink[] = "vm_help_link";                // text of a link
inline constexpr char kColourCodeBg[] = "vm_help_code_bg";           // background of a code span
inline constexpr char kColourTableHeadBg[] = "vm_help_th_bg";        // background of a table header
inline constexpr char kColourTableHeadText[] = "vm_help_th_text";    // text of a table header
inline constexpr char kColourMatchBg[] = "vm_help_match_bg";         // background of a search match
inline constexpr char kColourCurrentBg[] = "vm_help_current_bg";     // the current search match
inline constexpr char kColourCurrentText[] = "vm_help_current_text"; // text on the current match

inline constexpr double kHelpMinZoom = 0.5;
inline constexpr double kHelpMaxZoom = 3.0;

// One Help page on the screen. Main loop only.
//
// Layout. The page is as wide as kManualColumns characters of the view's font at the current zoom
// (page_width()), centred in the space the window gives it. When that space is narrower than the
// page, the page keeps its width and the window scrolls sideways; the text never re-wraps to
// another width, so the tables built by core/manual_table.h stay whole. A line that is longer than
// the page wraps after exactly kManualColumns characters (headings, bigger than the body text,
// wrap sooner). Everything is selectable and copyable; nothing can be edited.
//
// A GtkTextView that is the direct child of a GtkScrolledWindow follows the window's width, so the
// widget has two scrolled windows: the outer one scrolls sideways (never up or down) and holds the
// inner one, which is exactly page_width() wide and centred; the inner one holds the view and
// scrolls it up and down (its bar is not drawn: a GtkScrollbar of its own sits at the right edge of
// the widget, so the bar stays in view when the page is wider than the window). The view scrolls
// itself, so the mouse wheel, the keys, a drag that selects text beyond the edge and
// gtk_text_view_scroll_to_mark all work as in any text view; a wheel turn over the empty space at
// either side of the page scrolls it too.
class HelpPageView {
public:
    // Called with the topic file of a link ("open-and-save.md") when the reader clicks it.
    using LinkHandler = std::function<void(const std::string& target)>;

    // Builds the widgets, empty, at zoom 1.0. `on_link` may be empty.
    explicit HelpPageView(LinkHandler on_link);

    ~HelpPageView();

    HelpPageView(const HelpPageView&) = delete;
    HelpPageView& operator=(const HelpPageView&) = delete;

    // The widget to put into a window (a box with the page and its vertical bar); floating until
    // the caller puts it into a container. The view shares the life of the widget: keep both for
    // as long as the page is shown, destroy this object when the window that holds the widget is
    // destroyed.
    GtkWidget* widget() const { return root_; }
    // The text view inside it (borrowed; for tests and for the highlights of a later step).
    GtkTextView* text_view() const { return view_; }
    // The adjustments that scroll the page up and down (the view's) and sideways (the outer
    // scrolled window's); borrowed.
    GtkAdjustment* vadjustment() const;
    GtkAdjustment* hadjustment() const;

    // Shows `page`: replaces the text, applies the tags, drops the selection and any scroll still
    // queued, and scrolls to the top. Link targets and offsets are those of `page`. A text mark
    // is put at the start of every heading.
    void show(const ManualText& page);
    // The page on the screen (empty text before the first show()).
    const ManualText& page() const { return page_; }

    // Scrolls so that the top of heading `index` (an index into page().headings) is at the top of
    // the window (as far as the end of the page allows), and returns true; false, and no scroll,
    // when there is no such heading. GTK lays a page out bit by bit and a line's position moves
    // until it has, so the scroll is tried at once and again every 50 ms until the position has
    // been right three times in a row (at most 60 tries); while the window is not shown it waits.
    // A later call, show() and the destructor end a scroll that is still trying.
    bool scroll_to_heading(std::size_t index);

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

    // Zoom (not saved; 1.0 at start): scales the text and the page width together. set_zoom limits
    // the value to kHelpMinZoom..kHelpMaxZoom and returns whether the zoom changed. zoom_in and
    // zoom_out move to the next step of this list: 0.5 0.7 0.85 1.0 1.15 1.3 1.5 2.0 2.5 3.0 (the
    // first one above, or below, the current zoom; false at the end of the list). reset_zoom is
    // set_zoom(1.0). The actions "page.zoom-in", "page.zoom-out" and "page.zoom-reset" of the
    // widget do the same, bound to Ctrl+= (also Ctrl++ and Ctrl+keypad plus), Ctrl+- (also Ctrl+
    // keypad minus) and Ctrl+0 (also Ctrl+keypad 0) while the keyboard focus is in the widget.
    double zoom() const { return zoom_; }
    bool set_zoom(double zoom);
    bool zoom_in();
    bool zoom_out();
    bool reset_zoom();

    // Reads the colours of the page from the stylesheet again (the names above, through the
    // style of the text view) and puts them into the text tags: heading and link text, the
    // background of code spans, the table header's background and text. A name that the
    // stylesheet does not define leaves that colour unset (the view's own text colour). Called at
    // construction and whenever the view is mapped; the window calls it when the palette changes,
    // because tag colours are not CSS rules and do not follow a new palette by themselves.
    void refresh_colours();

    // The width of the page in pixels at the current zoom: 101.5 advances of the digit "0" in the
    // view's font at that zoom, rounded (GTK wraps a line of this width after exactly
    // kManualColumns characters; a line needs one character of room beyond its last column).
    int page_width() const;
    // The height of one line of body text at the current zoom, in pixels.
    int line_height() const;
    // The height of `lines` lines of body text plus the margins above and below, in pixels.
    int height_for_lines(int lines) const;

private:
    void build_tags();
    // Construction, in order.
    void build_widgets();
    void connect_pointer();
    void connect_wheel();
    void build_zoom_actions();
    void apply_zoom();
    void apply_page_tags();
    void on_click(double x, double y);
    void on_motion(double x, double y);
    // The character under the point (x, y) of the widget, or page().length.
    std::size_t offset_at(double x, double y) const;
    void on_wheel(double dy);
    void stop_heading_scroll();
    static gboolean run_heading_scroll(gpointer data);

    LinkHandler on_link_;
    ManualText page_;
    double zoom_ = 1.0;
    bool pointer_on_link_ = false;

    GtkWidget* root_ = nullptr;   // borrowed; floating, then owned by its container
    GtkWidget* outer_ = nullptr;  // borrowed; the root box owns it: scrolls sideways
    GtkWidget* inner_ = nullptr;  // borrowed; the outer window owns it: page_width() wide
    GtkTextView* view_ = nullptr; // borrowed; the inner window owns it
    GtkTextBuffer* buffer_ = nullptr;
    std::vector<GtkTextMark*> heading_marks_; // borrowed; the buffer owns them; one per heading
    std::size_t scroll_index_ = 0;            // the heading a running scroll goes to
    int scroll_steady_ = 0;                   // tries in a row at which the position was right
    int scroll_ticks_ = 0;                    // tries so far
    int scroll_waits_ = 0;                    // ticks spent waiting for the window to be shown
    unsigned scroll_source_ = 0;              // GLib source id of the running scroll; 0 if none

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
    gtkx::SignalConnection wheel_signal_;
    gtkx::SignalConnection map_signal_;
    gtkx::SignalConnection zoom_in_signal_;
    gtkx::SignalConnection zoom_out_signal_;
    gtkx::SignalConnection zoom_reset_signal_;
};

} // namespace vermilion::ui
