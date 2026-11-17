// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the Find window and connects its widgets to the shared SearchController.

#include "ui/find_window.h"

#include <gtk/gtk.h>

#include <string>
#include <utility>

namespace vermilion::ui {
namespace {

constexpr int kWindowWidth = 460;
constexpr int kEntryWidthChars = 30;
constexpr int kMargin = 10;
constexpr int kRowSpacing = 6;
constexpr int kColumnSpacing = 10;
constexpr int kStatusWidthChars = 40;

bool is_enter(guint keyval) {
    return keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter || keyval == GDK_KEY_ISO_Enter;
}

} // namespace

FindWindow::FindWindow(GtkWindow* parent, SearchController& controller, Host host)
    : controller_(controller), host_(std::move(host)),
      window_(GTK_WINDOW(gtk_window_new())) { // GTK owns the window; it is borrowed
    build(parent);
    connect_signals();
    connect_keys();
}

FindWindow::~FindWindow() {
    if (listening_) {
        controller_.set_on_status_changed(nullptr);
    }
}

bool FindWindow::is_open() const {
    return gtk_widget_get_visible(GTK_WIDGET(window_)) != FALSE;
}

void FindWindow::build(GtkWindow* parent) {
    gtk_window_set_title(window_, "Find");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_window_set_default_size(window_, kWindowWidth, -1);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-find");

    GtkWidget* grid = gtk_grid_new(); // floating; the window takes it over
    gtk_grid_set_row_spacing(GTK_GRID(grid), kRowSpacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), kColumnSpacing);
    gtk_widget_set_margin_top(grid, kMargin);
    gtk_widget_set_margin_bottom(grid, kMargin);
    gtk_widget_set_margin_start(grid, kMargin);
    gtk_widget_set_margin_end(grid, kMargin);

    GtkWidget* label = gtk_label_new_with_mnemonic("_Find what:");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    entry_ = gtk_entry_new(); // floating; the grid takes it over
    gtk_editable_set_width_chars(GTK_EDITABLE(entry_), kEntryWidthChars);
    gtk_widget_set_hexpand(entry_, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(entry_), TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_, 1, 0, 1, 1);

    find_next_ = gtk_button_new_with_mnemonic("Find _Next"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), find_next_, 2, 0, 1, 1);
    find_previous_ = gtk_button_new_with_mnemonic("Find _Previous");
    gtk_grid_attach(GTK_GRID(grid), find_previous_, 2, 1, 1, 1);
    close_ = gtk_button_new_with_mnemonic("Cl_ose");
    gtk_grid_attach(GTK_GRID(grid), close_, 2, 2, 1, 1);

    gtk_window_set_default_widget(window_, find_next_);
    build_options(grid);
    gtk_window_set_child(window_, grid);
}

void FindWindow::build_options(GtkWidget* grid) {
    match_case_ =
        gtk_check_button_new_with_mnemonic("Match _case"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), match_case_, 0, 1, 2, 1);
    whole_word_ = gtk_check_button_new_with_mnemonic("Match _whole word only");
    gtk_grid_attach(GTK_GRID(grid), whole_word_, 0, 2, 2, 1);
    regex_ = gtk_check_button_new_with_mnemonic("Regular e_xpression");
    gtk_grid_attach(GTK_GRID(grid), regex_, 0, 3, 2, 1);

    status_ = gtk_label_new(""); // floating; the grid takes it over
    gtk_label_set_xalign(GTK_LABEL(status_), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(status_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(status_), kStatusWidthChars);
    gtk_widget_add_css_class(status_, "vm-find-status");
    gtk_grid_attach(GTK_GRID(grid), status_, 0, 4, 3, 1);
}

void FindWindow::connect_signals() {
    signals_.push_back(gtkx::connect<void(GtkEditable*)>(
        entry_, "changed", [this](GtkEditable*) { on_text_changed(); }));
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        match_case_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                controller_.set_match_case(gtk_check_button_get_active(button) != FALSE);
            }
        }));
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        whole_word_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                controller_.set_whole_word(gtk_check_button_get_active(button) != FALSE);
            }
        }));
    signals_.push_back(
        gtkx::connect<void(GtkCheckButton*)>(regex_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                controller_.set_regex(gtk_check_button_get_active(button) != FALSE);
            }
        }));
    signals_.push_back(gtkx::connect<void(GtkButton*)>(find_next_, "clicked",
                                                       [this](GtkButton*) { find_next(); }));
    signals_.push_back(gtkx::connect<void(GtkButton*)>(find_previous_, "clicked",
                                                       [this](GtkButton*) { find_previous(); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(close_, "clicked", [this](GtkButton*) { close(); }));
    signals_.push_back(gtkx::connect<gboolean(GtkWindow*)>(window_, "close-request",
                                                           [this](GtkWindow*) -> gboolean {
                                                               close();
                                                               return TRUE;
                                                           }));
    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        window_, "notify::is-active", [this](GObject*, GParamSpec*) {
            if (gtk_window_is_active(window_)) {
                controller_.refresh_status();
                update_status();
            }
        }));
}

void FindWindow::connect_keys() {
    GtkEventController* entry_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(entry_keys, GTK_PHASE_CAPTURE);
    signals_.push_back(
        gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
            entry_keys, "key-pressed",
            [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType state) -> gboolean {
                if (is_enter(keyval) && (state & GDK_SHIFT_MASK) != 0 &&
                    (state & GDK_CONTROL_MASK) == 0 && (state & GDK_ALT_MASK) == 0) {
                    find_previous();
                    return TRUE;
                }
                return FALSE;
            }));
    gtk_widget_add_controller(entry_, entry_keys);

    GtkEventController* window_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(window_keys, GTK_PHASE_CAPTURE);
    signals_.push_back(
        gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
            window_keys, "key-pressed",
            [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType state) -> gboolean {
                if (keyval == GDK_KEY_Escape) {
                    close();
                    return TRUE;
                }
                if (keyval == GDK_KEY_F3 && (state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) == 0) {
                    if ((state & GDK_SHIFT_MASK) != 0) {
                        find_previous();
                    } else {
                        find_next();
                    }
                    return TRUE;
                }
                const bool control_h = (keyval == GDK_KEY_h || keyval == GDK_KEY_H) &&
                                       (state & GDK_CONTROL_MASK) != 0 &&
                                       (state & GDK_ALT_MASK) == 0;
                if (control_h && host_.show_replace) {
                    host_.show_replace();
                    return TRUE;
                }
                return FALSE;
            }));
    gtk_widget_add_controller(GTK_WIDGET(window_), window_keys);
}

void FindWindow::on_text_changed() {
    if (syncing_) {
        return;
    }
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry_)); // borrowed; copied by set_text
    controller_.set_text(text);
}

void FindWindow::sync_widgets(const std::string& text) {
    syncing_ = true;
    gtk_editable_set_text(GTK_EDITABLE(entry_), text.c_str());
    const SearchOptions& options = controller_.options();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(match_case_), options.match_case ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(whole_word_), options.whole_word ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(regex_), options.regex ? TRUE : FALSE);
    syncing_ = false;
}

void FindWindow::open(const std::optional<std::string>& selection) {
    if (selection.has_value()) {
        sync_widgets(*selection);
        controller_.set_text(*selection);
        controller_.apply();
    } else {
        sync_widgets(controller_.text());
    }
    controller_.set_on_status_changed([this] { update_status(); });
    listening_ = true;
    controller_.refresh_status();
    update_status();
    gtk_window_present(window_);
    gtk_widget_grab_focus(entry_);
    gtk_editable_select_region(GTK_EDITABLE(entry_), 0, -1);
}

void FindWindow::close() {
    if (!is_open()) {
        return;
    }
    hide();
    if (host_.closed) {
        host_.closed();
    }
}

void FindWindow::hide() {
    if (!is_open()) {
        return;
    }
    if (listening_) {
        controller_.set_on_status_changed(nullptr);
        listening_ = false;
    }
    gtk_widget_set_visible(GTK_WIDGET(window_), FALSE);
}

void FindWindow::find_next() {
    FindResult result = controller_.find_next();
    if (result.found) {
        scroll_to_match();
    }
}

void FindWindow::find_previous() {
    FindResult result = controller_.find_previous();
    if (result.found) {
        scroll_to_match();
    }
}

void FindWindow::update_status() {
    gtk_label_set_text(GTK_LABEL(status_), controller_.status().c_str());
    if (controller_.status_is_problem()) {
        gtk_widget_add_css_class(status_, "vm-find-problem");
    } else {
        gtk_widget_remove_css_class(status_, "vm-find-problem");
    }
}

void FindWindow::scroll_to_match() {
    if (!host_.active_view) {
        return;
    }
    GtkTextView* view = host_.active_view();
    if (view == nullptr) {
        return;
    }
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(view); // borrowed
    gtk_text_view_scroll_to_mark(view, gtk_text_buffer_get_insert(buffer), 0.1, FALSE, 0.0, 0.0);
}

} // namespace vermilion::ui
