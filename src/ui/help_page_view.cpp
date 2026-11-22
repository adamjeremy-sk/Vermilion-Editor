// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the Help page view: the text view, its tags and its links.

#include "ui/help_page_view.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
// The page width in advances of the digit "0": kManualColumns plus one column of room.
constexpr double kPageColumnsWithRoom = static_cast<double>(kManualColumns) + 1.5;
// The steps of zoom in and out, increasing.
constexpr double kZoomSteps[] = {0.5, 0.7, 0.85, 1.0, 1.15, 1.3, 1.5, 2.0, 2.5, 3.0};
// A scroll to a heading is tried every kScrollTickMs ms until the position has been right
// kScrollSteadyTicks tries in a row, at most kScrollMaxTicks tries; while the window is not shown
// it waits, at most kScrollMaxWaits ticks.
constexpr guint kScrollTickMs = 50;
constexpr int kScrollSteadyTicks = 3;
constexpr int kScrollMaxTicks = 60;
constexpr int kScrollMaxWaits = 200;

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

// Adds a Ctrl+key shortcut that activates `action` to the shortcut `controller`.
void add_shortcut(GtkEventController* controller, guint key, const char* action) {
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(gtk_keyval_trigger_new(key, GDK_CONTROL_MASK),
                         gtk_named_action_new(action)));
}

} // namespace

HelpPageView::HelpPageView(LinkHandler on_link) : on_link_(std::move(on_link)) {
    build_widgets();
    buffer_ = gtk_text_view_get_buffer(view_); // borrowed
    build_tags();
    connect_pointer();
    connect_wheel();
    // The font in use is known only once the view has a style; measure it then and now.
    map_signal_ = gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(view_), "map",
                                                  [this](GtkWidget*) { apply_zoom(); });
    build_zoom_actions();
    apply_zoom();
}

HelpPageView::~HelpPageView() {
    stop_heading_scroll();
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
    gtk_widget_set_halign(inner_, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(inner_, TRUE);

    GtkWidget* view = gtk_text_view_new(); // floating; the inner window owns it
    view_ = GTK_TEXT_VIEW(view);
    gtk_widget_add_css_class(view, kHelpViewClass);
    gtk_text_view_set_editable(view_, FALSE);
    gtk_text_view_set_cursor_visible(view_, FALSE);
    gtk_text_view_set_wrap_mode(view_, GTK_WRAP_CHAR);
    gtk_widget_set_margin_start(view, 0);
    gtk_widget_set_margin_end(view, 0);
    gtk_widget_set_margin_top(view, kHelpPageMargin);
    gtk_widget_set_margin_bottom(view, kHelpPageMargin);
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

void HelpPageView::connect_wheel() {
    GtkEventController* wheel =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL); // floating
    gtk_event_controller_set_name(wheel, kHelpScrollController);
    gtk_widget_add_controller(outer_, wheel); // the outer window owns it
    wheel_signal_ = gtkx::connect<gboolean(GtkEventControllerScroll*, double, double)>(
        wheel, "scroll", [this](GtkEventControllerScroll*, double, double dy) -> gboolean {
            on_wheel(dy);
            return TRUE;
        });
}

void HelpPageView::build_zoom_actions() {
    GSimpleActionGroup* group = g_simple_action_group_new();
    GSimpleAction* action_in = g_simple_action_new("zoom-in", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_in));
    zoom_in_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_in, "activate", [this](GSimpleAction*, GVariant*) { zoom_in(); });
    g_object_unref(action_in); // the map holds it
    GSimpleAction* action_out = g_simple_action_new("zoom-out", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_out));
    zoom_out_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_out, "activate", [this](GSimpleAction*, GVariant*) { zoom_out(); });
    g_object_unref(action_out); // the map holds it
    GSimpleAction* action_reset = g_simple_action_new("zoom-reset", nullptr);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action_reset));
    zoom_reset_signal_ = gtkx::connect<void(GSimpleAction*, GVariant*)>(
        action_reset, "activate", [this](GSimpleAction*, GVariant*) { reset_zoom(); });
    g_object_unref(action_reset); // the map holds it
    gtk_widget_insert_action_group(root_, "page", G_ACTION_GROUP(group));
    g_object_unref(group);

    GtkEventController* keys = gtk_shortcut_controller_new(); // floating; the widget owns it
    add_shortcut(keys, GDK_KEY_equal, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_plus, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_KP_Add, "page.zoom-in");
    add_shortcut(keys, GDK_KEY_minus, "page.zoom-out");
    add_shortcut(keys, GDK_KEY_KP_Subtract, "page.zoom-out");
    add_shortcut(keys, GDK_KEY_0, "page.zoom-reset");
    add_shortcut(keys, GDK_KEY_KP_0, "page.zoom-reset");
    gtk_widget_add_controller(root_, keys);
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
    table_header_ =
        gtk_text_buffer_create_tag(buffer_, kTagTableHeader, "weight", PANGO_WEIGHT_BOLD, nullptr);
    bold_ = gtk_text_buffer_create_tag(buffer_, kTagBold, "weight", PANGO_WEIGHT_BOLD, nullptr);
    code_ = gtk_text_buffer_create_tag(buffer_, kTagCode, nullptr);
    link_ =
        gtk_text_buffer_create_tag(buffer_, kTagLink, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
    heading_[0] = gtk_text_buffer_create_tag(buffer_, kTagHeading1, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", kHeading1Scale, nullptr);
    heading_[1] = gtk_text_buffer_create_tag(buffer_, kTagHeading2, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", kHeading2Scale, nullptr);
    heading_[2] = gtk_text_buffer_create_tag(buffer_, kTagHeading3, "weight", PANGO_WEIGHT_BOLD,
                                             "scale", 1.0, nullptr);
}

void HelpPageView::apply_zoom() {
    g_object_set(body_, "scale", zoom_, nullptr);
    g_object_set(heading_[0], "scale", kHeading1Scale * zoom_, nullptr);
    g_object_set(heading_[1], "scale", kHeading2Scale * zoom_, nullptr);
    g_object_set(heading_[2], "scale", zoom_, nullptr);
    const Measure font = measure_font(GTK_WIDGET(view_), zoom_);
    const int indent = static_cast<int>(lround(kItemIndentColumns * font.advance));
    g_object_set(item_, "indent", -indent, nullptr);
    gtk_widget_set_size_request(inner_, page_width(), -1);
}

int HelpPageView::page_width() const {
    return static_cast<int>(
        lround(kPageColumnsWithRoom * measure_font(GTK_WIDGET(view_), zoom_).advance));
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
    stop_heading_scroll();
    for (GtkTextMark* mark : heading_marks_) {
        gtk_text_buffer_delete_mark(buffer_, mark);
    }
    heading_marks_.clear();
    page_ = page;
    gtk_text_buffer_set_text(buffer_, valid_text(page_.text).c_str(), -1);
    apply_page_tags();
    for (const auto& heading : page_.headings) {
        GtkTextIter at;
        gtk_text_buffer_get_iter_at_offset(buffer_, &at, static_cast<int>(heading.begin));
        heading_marks_.push_back(gtk_text_buffer_create_mark(buffer_, nullptr, &at, TRUE));
    }
    hover_at(page_.length);
    gtk_adjustment_set_value(vadjustment(), 0.0);
}

bool HelpPageView::scroll_to_heading(std::size_t index) {
    if (index >= heading_marks_.size()) {
        return false;
    }
    stop_heading_scroll();
    scroll_index_ = index;
    scroll_steady_ = 0;
    scroll_ticks_ = 0;
    scroll_waits_ = 0;
    scroll_source_ = g_timeout_add(kScrollTickMs, &HelpPageView::run_heading_scroll, this);
    run_heading_scroll(this); // one try at once
    return true;
}

void HelpPageView::stop_heading_scroll() {
    if (scroll_source_ != 0) {
        g_source_remove(scroll_source_);
        scroll_source_ = 0;
    }
}

gboolean HelpPageView::run_heading_scroll(gpointer data) {
    HelpPageView* self = static_cast<HelpPageView*>(data);
    if (!gtk_widget_get_mapped(self->root_)) {
        if (++self->scroll_waits_ >= kScrollMaxWaits) {
            self->scroll_source_ = 0;
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    }
    GtkTextIter at;
    gtk_text_buffer_get_iter_at_mark(self->buffer_, &at, self->heading_marks_[self->scroll_index_]);
    GdkRectangle where;
    gtk_text_view_get_iter_location(self->view_, &at, &where);
    GtkAdjustment* adjustment = self->vadjustment();
    const double last =
        gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);
    const double want =
        std::max(0.0, std::min(static_cast<double>(where.y + kHelpPageMargin), last));
    const double value = gtk_adjustment_get_value(adjustment);
    if (std::fabs(value - want) > 0.5) {
        gtk_adjustment_set_value(adjustment, want);
        self->scroll_steady_ = 0;
    } else {
        ++self->scroll_steady_;
    }
    if (self->scroll_steady_ >= kScrollSteadyTicks || ++self->scroll_ticks_ >= kScrollMaxTicks) {
        self->scroll_source_ = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
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

void HelpPageView::on_wheel(double dy) {
    GtkAdjustment* adjustment = vadjustment();
    const double step = std::pow(gtk_adjustment_get_page_size(adjustment), 2.0 / 3.0);
    gtk_adjustment_set_value(adjustment, gtk_adjustment_get_value(adjustment) + dy * step);
}

bool HelpPageView::set_zoom(double zoom) {
    const double clamped = std::min(kHelpMaxZoom, std::max(kHelpMinZoom, zoom));
    if (clamped == zoom_) {
        return false;
    }
    zoom_ = clamped;
    apply_zoom();
    return true;
}

bool HelpPageView::zoom_in() {
    for (double step : kZoomSteps) {
        if (step > zoom_ + 1e-9) {
            return set_zoom(step);
        }
    }
    return false;
}

bool HelpPageView::zoom_out() {
    for (std::size_t i = std::size(kZoomSteps); i > 0; --i) {
        if (kZoomSteps[i - 1] < zoom_ - 1e-9) {
            return set_zoom(kZoomSteps[i - 1]);
        }
    }
    return false;
}

bool HelpPageView::reset_zoom() {
    return set_zoom(1.0);
}

} // namespace vermilion::ui
