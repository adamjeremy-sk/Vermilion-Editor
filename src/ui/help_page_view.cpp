// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the Help page view: the text view, its tags, its links and its page layout.

#include "ui/help_page_view.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {
namespace {

// The size of an h1 heading relative to the body text (a multiple of the zoom).
constexpr double kHeading1Scale = 1.5;
// The size of an h2 heading relative to the body text (a multiple of the zoom).
constexpr double kHeading2Scale = 1.25;
// The hanging indent of a list item, in columns of the view's font.
constexpr int kItemIndentColumns = 2;

// Bad bytes become U+FFFD, so GTK never warns about the text.
std::string valid_text(const std::string& text) {
    return gtkx::take_string(g_utf8_make_valid(text.c_str(), static_cast<gssize>(text.size())));
}

// The advance of one digit and the height of one line, in the view's font.
struct Measure {
    double advance = 0.0;
    int height = 0;
};

// The advance of one digit and the height of one line, in the view's font at `zoom`.
Measure measure_font(GtkWidget* view, double zoom) {
    gtkx::ObjectPtr<PangoLayout> layout =
        gtkx::ObjectPtr<PangoLayout>::adopt(gtk_widget_create_pango_layout(view, "0000000000"));
    PangoAttrList* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_scale_new(zoom));
    pango_layout_set_attributes(layout.get(), attributes);
    pango_attr_list_unref(attributes);
    int width = 0;
    int height = 0;
    pango_layout_get_size(layout.get(), &width, &height);
    Measure m;
    m.advance = static_cast<double>(width) / (10.0 * PANGO_SCALE);
    m.height = static_cast<int>(std::ceil(static_cast<double>(height) / PANGO_SCALE));
    return m;
}

} // namespace

HelpPageView::HelpPageView(LinkHandler on_link) : on_link_(std::move(on_link)) {
    build_widgets();
    buffer_ = gtk_text_view_get_buffer(view_); // borrowed
    build_tags();
    connect_pointer();
    connect_wheel();
    // The font in use is known only once the view has a style; measure it then and now.
    map_signal_ = gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(view_), "map", [this](GtkWidget*) {
        apply_zoom();
        relayout(reading_offset());
        refresh_colours();
    });
    width_signal_ = gtkx::connect<void(GObject*, GParamSpec*)>(
        hadjustment(), "notify::page-size", [this](GObject*, GParamSpec*) { on_width(); });
    refresh_colours();
    build_zoom_actions();
    apply_zoom();
}

HelpPageView::~HelpPageView() {
    stop_heading_scroll();
    stop_relayout();
}

void HelpPageView::build_widgets() {
    root_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); // floating; the caller's container owns it
    outer_ = gtk_scrolled_window_new();                 // floating; the root box owns it
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(outer_), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_NEVER);
    gtk_widget_set_hexpand(outer_, TRUE);
    gtk_widget_set_vexpand(outer_, TRUE);
    inner_ = gtk_scrolled_window_new(); // floating; the outer window owns it
    gtk_widget_add_css_class(inner_, kHelpHolderClass);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(inner_), GTK_POLICY_NEVER,
                                   GTK_POLICY_EXTERNAL);
    gtk_widget_set_vexpand(inner_, TRUE);

    GtkWidget* view = gtk_text_view_new(); // floating; the inner window owns it
    view_ = GTK_TEXT_VIEW(view);
    gtk_widget_add_css_class(view, kHelpViewClass);
    gtk_text_view_set_editable(view_, FALSE);
    gtk_text_view_set_cursor_visible(view_, FALSE);
    gtk_text_view_set_wrap_mode(view_, GTK_WRAP_CHAR);
    // The margins belong to the text view, not to the widget: a widget margin clips the top of a
    // heading that is scrolled to the top.
    gtk_text_view_set_left_margin(view_, 0);
    gtk_text_view_set_right_margin(view_, 0);
    gtk_text_view_set_top_margin(view_, kHelpPageMargin);
    gtk_text_view_set_bottom_margin(view_, kHelpPageMargin);
    gtk_widget_set_cursor_from_name(view, "text");

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(inner_), view);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(outer_), inner_);
    GtkWidget* bar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadjustment()); // floating
    gtk_box_append(GTK_BOX(root_), outer_);
    gtk_box_append(GTK_BOX(root_), bar);
}

void HelpPageView::connect_pointer() {
    GtkWidget* view = GTK_WIDGET(view_);
    // A click on the view, and the pointer over it (both controllers are owned by the view).
    GtkGesture* click = gtk_gesture_click_new(); // floating; the view owns it from the add
    gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(click), kHelpClickController);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(view, GTK_EVENT_CONTROLLER(click));
    click_signal_ = gtkx::connect<void(GtkGestureClick*, int, double, double)>(
        click, "released", [this](GtkGestureClick*, int, double x, double y) { on_click(x, y); });
    GtkEventController* motion = gtk_event_controller_motion_new(); // floating; the view owns it
    gtk_event_controller_set_name(motion, kHelpMotionController);
    gtk_widget_add_controller(view, motion);
    motion_signal_ = gtkx::connect<void(GtkEventControllerMotion*, double, double)>(
        motion, "motion",
        [this](GtkEventControllerMotion*, double x, double y) { on_motion(x, y); });
    leave_signal_ = gtkx::connect<void(GtkEventControllerMotion*)>(
        motion, "leave", [this](GtkEventControllerMotion*) { hover_at(page_.length); });
}

GtkAdjustment* HelpPageView::vadjustment() const {
    return gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(inner_));
}

GtkAdjustment* HelpPageView::hadjustment() const {
    return gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(outer_));
}

void HelpPageView::build_tags() {
    body_ = gtk_text_buffer_create_tag(buffer_, kTagBody, "scale", 1.0, nullptr);
    item_ = gtk_text_buffer_create_tag(buffer_, kTagItem, nullptr);
    bold_ = gtk_text_buffer_create_tag(buffer_, kTagBold, "weight", PANGO_WEIGHT_BOLD, nullptr);
    code_ = gtk_text_buffer_create_tag(buffer_, kTagCode, nullptr);
    link_ = gtk_text_buffer_create_tag(buffer_, kTagLink, nullptr);
    // Created after the code and link tags, so that its colours win inside a table header.
    table_header_ =
        gtk_text_buffer_create_tag(buffer_, kTagTableHeader, "weight", PANGO_WEIGHT_BOLD, nullptr);
    heading_[0] = gtk_text_buffer_create_tag(buffer_, kTagHeading1, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", kHeading1Scale, nullptr);
    heading_[1] = gtk_text_buffer_create_tag(buffer_, kTagHeading2, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", kHeading2Scale, nullptr);
    heading_[2] = gtk_text_buffer_create_tag(buffer_, kTagHeading3, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", 1.0, nullptr);
    // Created last, so that their colours win over those of the other tags; the current match
    // after the others, so that it wins over a plain match.
    match_tag_ = gtk_text_buffer_create_tag(buffer_, kTagMatch, nullptr);
    current_tag_ = gtk_text_buffer_create_tag(buffer_, kTagCurrentMatch, nullptr);
}

void HelpPageView::apply_zoom() {
    g_object_set(body_, "scale", zoom_, nullptr);
    g_object_set(heading_[0], "scale", kHeading1Scale * zoom_, nullptr);
    g_object_set(heading_[1], "scale", kHeading2Scale * zoom_, nullptr);
    g_object_set(heading_[2], "scale", zoom_, nullptr);
    const Measure font = measure_font(GTK_WIDGET(view_), zoom_);
    const int indent = static_cast<int>(lround(kItemIndentColumns * font.advance));
    g_object_set(item_, "indent", -indent, nullptr);
    gtk_widget_set_size_request(inner_, min_width(), -1);
}

double HelpPageView::advance() const {
    return measure_font(GTK_WIDGET(view_), zoom_).advance;
}

int HelpPageView::line_height() const {
    return measure_font(GTK_WIDGET(view_), zoom_).height;
}

int HelpPageView::height_for_lines(int lines) const {
    return lines * line_height() + 2 * kHelpPageMargin;
}

void HelpPageView::apply_page_tags() {
    const auto apply = [this](GtkTextTag* tag, std::size_t begin, std::size_t end) {
        GtkTextIter from;
        GtkTextIter to;
        gtk_text_buffer_get_iter_at_offset(buffer_, &from, static_cast<int>(begin));
        gtk_text_buffer_get_iter_at_offset(buffer_, &to, static_cast<int>(end));
        gtk_text_buffer_apply_tag(buffer_, tag, &from, &to);
    };
    apply(body_, 0, page_.length);
    for (const auto& item : page_.list_items) {
        apply(item_, item.begin, item.end);
    }
    for (const auto& header : page_.table_headers) {
        apply(table_header_, header.begin, header.end);
    }
    for (const auto& span : page_.bold) {
        apply(bold_, span.begin, span.end);
    }
    for (const auto& span : page_.code) {
        apply(code_, span.begin, span.end);
    }
    for (const auto& link : page_.links) {
        apply(link_, link.begin, link.end);
    }
    for (const auto& heading : page_.headings) {
        GtkTextTag* tag = heading_[2];
        if (heading.level == 1) {
            tag = heading_[0];
        } else if (heading.level == 2) {
            tag = heading_[1];
        }
        apply(tag, heading.begin, heading.end);
    }
}

void HelpPageView::show(const ManualText& page) {
    stop_relayout();
    source_ = ManualPage{};
    has_source_ = false;
    min_columns_ = 0;
    gtk_widget_set_size_request(inner_, min_width(), -1);
    set_text(page);
}

void HelpPageView::set_text(const ManualText& page, std::optional<std::size_t> keep_match_at) {
    stop_heading_scroll();
    for (GtkTextMark* mark : heading_marks_) {
        gtk_text_buffer_delete_mark(buffer_, mark);
    }
    heading_marks_.clear();
    if (anchor_mark_ != nullptr) {
        gtk_text_buffer_delete_mark(buffer_, anchor_mark_);
        anchor_mark_ = nullptr;
    }
    page_ = page;
    gtk_text_buffer_set_text(buffer_, valid_text(page_.text).c_str(), -1);
    apply_page_tags();
    for (const auto& heading : page_.headings) {
        GtkTextIter at;
        gtk_text_buffer_get_iter_at_offset(buffer_, &at, static_cast<int>(heading.begin));
        heading_marks_.push_back(gtk_text_buffer_create_mark(buffer_, nullptr, &at, TRUE));
    }
    highlight_page(keep_match_at);
    hover_at(page_.length);
    gtk_adjustment_set_value(vadjustment(), 0.0);
}

std::size_t HelpPageView::offset_at(double x, double y) const {
    int bx = 0;
    int by = 0;
    gtk_text_view_window_to_buffer_coords(view_, GTK_TEXT_WINDOW_WIDGET, static_cast<int>(x),
                                          static_cast<int>(y), &bx, &by);
    GtkTextIter iter;
    gtk_text_view_get_iter_at_location(view_, &iter, bx, by);
    return static_cast<std::size_t>(gtk_text_iter_get_offset(&iter));
}

bool HelpPageView::activate_at(std::size_t offset) {
    const LinkRange* link = link_at(page_, offset);
    if (link == nullptr) {
        return false;
    }
    if (on_link_) {
        const std::string target = link->target; // the handler may show another page
        on_link_(target);
    }
    return true;
}

void HelpPageView::hover_at(std::size_t offset) {
    pointer_on_link_ = link_at(page_, offset) != nullptr;
    gtk_widget_set_cursor_from_name(GTK_WIDGET(view_), pointer_on_link_ ? "pointer" : "text");
}

void HelpPageView::on_click(double x, double y) {
    if (gtk_text_buffer_get_has_selection(buffer_)) {
        return; // a drag that selected text is not a click
    }
    activate_at(offset_at(x, y));
}

void HelpPageView::on_motion(double x, double y) {
    hover_at(offset_at(x, y));
}

} // namespace vermilion::ui
