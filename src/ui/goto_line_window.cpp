// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the Go to Line window and jumps to the typed line on the active document.

#include "ui/goto_line_window.h"

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <string>
#include <utility>

#include "ui/line_jump.h"

namespace vermilion::ui {
namespace {

constexpr int kWindowWidth = 380;
constexpr int kEntryWidthChars = 20;
constexpr int kMargin = 10;
constexpr int kRowSpacing = 6;
constexpr int kColumnSpacing = 10;
constexpr int kStatusWidthChars = 40;

} // namespace

GotoLineWindow::GotoLineWindow(GtkWindow* parent, Host host)
    : host_(std::move(host)), window_(GTK_WINDOW(gtk_window_new())) { // GTK owns the window
    build(parent);
    connect_signals();
    connect_keys();
}

bool GotoLineWindow::is_open() const {
    return gtk_widget_get_visible(GTK_WIDGET(window_)) != FALSE;
}

void GotoLineWindow::build(GtkWindow* parent) {
    gtk_window_set_title(window_, "Go to Line");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_window_set_default_size(window_, kWindowWidth, -1);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-goto");

    GtkWidget* grid = gtk_grid_new(); // floating; the window takes it over
    gtk_grid_set_row_spacing(GTK_GRID(grid), kRowSpacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), kColumnSpacing);
    gtk_widget_set_margin_top(grid, kMargin);
    gtk_widget_set_margin_bottom(grid, kMargin);
    gtk_widget_set_margin_start(grid, kMargin);
    gtk_widget_set_margin_end(grid, kMargin);

    GtkWidget* label = gtk_label_new_with_mnemonic("_Line or line:column:");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    entry_ = gtk_entry_new(); // floating; the grid takes it over
    gtk_editable_set_width_chars(GTK_EDITABLE(entry_), kEntryWidthChars);
    gtk_widget_set_hexpand(entry_, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(entry_), TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_, 1, 0, 1, 1);

    go_ = gtk_button_new_with_mnemonic("_Go"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), go_, 2, 0, 1, 1);

    range_ = gtk_label_new(""); // floating; the grid takes it over
    gtk_label_set_xalign(GTK_LABEL(range_), 0.0f);
    gtk_grid_attach(GTK_GRID(grid), range_, 0, 1, 2, 1);

    cancel_ = gtk_button_new_with_mnemonic("_Cancel"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), cancel_, 2, 1, 1, 1);

    status_ = gtk_label_new(""); // floating; the grid takes it over
    gtk_label_set_xalign(GTK_LABEL(status_), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(status_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(status_), kStatusWidthChars);
    gtk_grid_attach(GTK_GRID(grid), status_, 0, 2, 3, 1);

    gtk_window_set_default_widget(window_, go_);
    gtk_window_set_child(window_, grid);
}

void GotoLineWindow::connect_signals() {
    signals_.push_back(gtkx::connect<void(GtkEditable*)>(
        entry_, "changed", [this](GtkEditable*) { show_status("", false); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(go_, "clicked", [this](GtkButton*) { go(); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(cancel_, "clicked", [this](GtkButton*) { close(); }));
    signals_.push_back(gtkx::connect<gboolean(GtkWindow*)>(window_, "close-request",
                                                           [this](GtkWindow*) -> gboolean {
                                                               close();
                                                               return TRUE;
                                                           }));
    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        window_, "notify::is-active", [this](GObject*, GParamSpec*) {
            if (gtk_window_is_active(window_)) {
                refresh_range();
            }
        }));
}

void GotoLineWindow::connect_keys() {
    GtkEventController* window_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(window_keys, GTK_PHASE_CAPTURE);
    signals_.push_back(
        gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
            window_keys, "key-pressed",
            [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType) -> gboolean {
                if (keyval == GDK_KEY_Escape) {
                    close();
                    return TRUE;
                }
                return FALSE;
            }));
    gtk_widget_add_controller(GTK_WIDGET(window_), window_keys);
}

void GotoLineWindow::open() {
    if (!host_.active_view) {
        return;
    }
    GtkTextView* view = host_.active_view();
    if (view == nullptr) {
        return;
    }
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
    gtk_editable_set_text(GTK_EDITABLE(entry_), std::to_string(current_line_of(buffer)).c_str());
    show_status("", false);
    refresh_range();
    gtk_window_present(window_);
    gtk_widget_grab_focus(entry_);
    gtk_editable_select_region(GTK_EDITABLE(entry_), 0, -1);
}

void GotoLineWindow::close() {
    if (!is_open()) {
        return;
    }
    gtk_widget_set_visible(GTK_WIDGET(window_), FALSE);
    if (host_.closed) {
        host_.closed();
    }
}

void GotoLineWindow::go() {
    if (!host_.active_view) {
        return;
    }
    GtkTextView* view = host_.active_view();
    if (view == nullptr) {
        return;
    }
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
    const unsigned tab_width = gtk_source_view_get_tab_width(GTK_SOURCE_VIEW(view));
    const std::string typed = gtk_editable_get_text(GTK_EDITABLE(entry_)); // borrowed; copied
    JumpResult result = jump_to_line(buffer, tab_width, typed);
    if (result.kind == JumpKind::Invalid) {
        show_status(result.message, true);
        gtk_widget_grab_focus(entry_);
        gtk_editable_select_region(GTK_EDITABLE(entry_), 0, -1);
        return;
    }
    scroll_to_cursor(view);
    if (result.kind == JumpKind::PastEnd && host_.message) {
        host_.message(result.message);
    }
    close();
}

void GotoLineWindow::refresh_range() {
    if (!host_.active_view) {
        gtk_label_set_text(GTK_LABEL(range_), "");
        return;
    }
    GtkTextView* view = host_.active_view();
    if (view == nullptr) {
        gtk_label_set_text(GTK_LABEL(range_), "");
        return;
    }
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
    gtk_label_set_text(GTK_LABEL(range_), ("Lines " + range_text_of(buffer)).c_str());
}

void GotoLineWindow::show_status(const std::string& text, bool problem) {
    gtk_label_set_text(GTK_LABEL(status_), text.c_str());
    if (problem) {
        gtk_widget_add_css_class(status_, "vm-find-problem");
    } else {
        gtk_widget_remove_css_class(status_, "vm-find-problem");
    }
}

void GotoLineWindow::scroll_to_cursor(GtkTextView* view) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
    gtk_text_view_scroll_to_mark(view, gtk_text_buffer_get_insert(buffer), 0.0, TRUE, 0.5, 0.5);
}

} // namespace vermilion::ui
