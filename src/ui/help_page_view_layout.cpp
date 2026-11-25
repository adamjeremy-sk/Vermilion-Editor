// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Implements the width of the Help page view: the columns that fit, the layout of a source page for
// them, and the new layout, with the reading position kept, when the width or the zoom changes.

#include <gtk/gtk.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "core/manual_table.h"
#include "core/manual_text.h"
#include "ui/help_page_view.h"

namespace vermilion::ui {
namespace {

// A change of the window's width is laid out this long after the last change.
constexpr guint kRelayoutMs = 100;
// A line needs this many advances of the digit "0" of room beyond its last column.
constexpr double kColumnRoom = 1.5;

} // namespace

int HelpPageView::width_for_columns(int columns) const {
    int bar = 0;
    gtk_widget_measure(gtk_widget_get_last_child(root_), GTK_ORIENTATION_HORIZONTAL, -1, nullptr,
                       &bar, nullptr, nullptr); // the root box holds the page, then the bar
    return static_cast<int>(std::ceil((columns + kColumnRoom) * advance())) + bar;
}

int HelpPageView::min_width() const {
    if (min_columns_ == 0) {
        return -1;
    }
    return static_cast<int>(
        std::ceil((static_cast<double>(min_columns_) + kColumnRoom) * advance()));
}

std::size_t HelpPageView::measure_columns() const {
    const double one = advance();
    const double window = gtk_adjustment_get_page_size(hadjustment());
    std::size_t columns = kHelpOpenColumns;
    if (window > 0.0 && one > 0.0) {
        const double width = std::max(window, static_cast<double>(min_width()));
        columns =
            static_cast<std::size_t>(std::max(1.0, std::floor(width / one - kColumnRoom + 1e-6)));
    }
    return std::max(columns, min_columns_);
}

void HelpPageView::show_source(const ManualPage& page) {
    stop_relayout();
    source_ = page;
    has_source_ = true;
    min_columns_ = page_min_columns(page);
    gtk_widget_set_size_request(inner_, min_width(), -1);
    columns_ = measure_columns();
    set_text(build_manual_text(source_, columns_));
}

std::size_t HelpPageView::reading_offset() const {
    if (relayout_source_ != 0) {
        return pending_offset_;
    }
    GtkTextIter at;
    if (scroll_source_ != 0 && scroll_mark_ != nullptr) {
        gtk_text_buffer_get_iter_at_mark(buffer_, &at, scroll_mark_);
    } else {
        GdkRectangle visible;
        gtk_text_view_get_visible_rect(view_, &visible);
        gtk_text_view_get_iter_at_location(view_, &at, visible.x, visible.y);
    }
    return static_cast<std::size_t>(gtk_text_iter_get_offset(&at));
}

void HelpPageView::relayout(std::size_t offset) {
    stop_relayout();
    if (!has_source_) {
        return;
    }
    const std::size_t columns = measure_columns();
    if (columns == columns_) {
        return;
    }
    const ManualText built = build_manual_text(source_, columns);
    const std::size_t target = corresponding_offset(page_, built, offset);
    std::optional<std::size_t> keep_match_at;
    if (current_match_.has_value()) {
        keep_match_at = corresponding_offset(page_, built, matches_[*current_match_].begin);
    }
    columns_ = columns;
    set_text(built, keep_match_at); // drops the selection and scrolls to the top
    scroll_to_offset(target);
}

void HelpPageView::request_relayout() {
    const std::size_t at = reading_offset(); // before the waiting layout is replaced
    stop_relayout();
    pending_offset_ = at;
    relayout_source_ = g_timeout_add(kRelayoutMs, &HelpPageView::run_relayout, this);
}

void HelpPageView::stop_relayout() {
    if (relayout_source_ != 0) {
        g_source_remove(relayout_source_);
        relayout_source_ = 0;
    }
}

gboolean HelpPageView::run_relayout(gpointer data) {
    HelpPageView* self = static_cast<HelpPageView*>(data);
    self->relayout_source_ = 0;
    self->relayout(self->pending_offset_);
    return G_SOURCE_REMOVE;
}

void HelpPageView::on_width() {
    const bool first = !width_seen_;
    width_seen_ = gtk_adjustment_get_page_size(hadjustment()) > 0.0;
    if (!has_source_ || measure_columns() == columns_) {
        return;
    }
    if (first) {
        relayout(reading_offset());
    } else {
        request_relayout();
    }
}

void HelpPageView::scroll_to_offset(std::size_t offset) {
    if (offset == 0) {
        return; // set_text() has scrolled to the top
    }
    GtkTextIter at;
    gtk_text_buffer_get_iter_at_offset(buffer_, &at, static_cast<int>(offset));
    anchor_mark_ = gtk_text_buffer_create_mark(buffer_, nullptr, &at, TRUE);
    start_scroll(anchor_mark_);
}

} // namespace vermilion::ui
