// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the Replace window and connects its widgets to the shared search and the replace logic.

#include "ui/replace_window.h"

#include <gtk/gtk.h>

#include <initializer_list>
#include <string>
#include <utility>

#include "core/search_messages.h"

namespace vermilion::ui {
namespace {

constexpr int kWindowWidth = 500;
constexpr int kEntryWidthChars = 30;
constexpr int kMargin = 10;
constexpr int kRowSpacing = 6;
constexpr int kColumnSpacing = 10;
constexpr int kStatusWidthChars = 44;

bool is_enter(guint keyval) {
    return keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter || keyval == GDK_KEY_ISO_Enter;
}

void build_entry_rows(GtkWidget* grid, GtkWidget** find_entry, GtkWidget** replace_entry) {
    GtkWidget* find_label = gtk_label_new_with_mnemonic("_Find what:");
    gtk_label_set_xalign(GTK_LABEL(find_label), 0.0f);
    *find_entry = gtk_entry_new(); // floating; the grid takes it over
    gtk_editable_set_width_chars(GTK_EDITABLE(*find_entry), kEntryWidthChars);
    gtk_widget_set_hexpand(*find_entry, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(*find_entry), TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(find_label), *find_entry);
    gtk_grid_attach(GTK_GRID(grid), find_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), *find_entry, 1, 0, 1, 1);

    GtkWidget* replace_label = gtk_label_new_with_mnemonic("Re_place with:");
    gtk_label_set_xalign(GTK_LABEL(replace_label), 0.0f);
    *replace_entry = gtk_entry_new(); // floating; the grid takes it over
    gtk_editable_set_width_chars(GTK_EDITABLE(*replace_entry), kEntryWidthChars);
    gtk_widget_set_hexpand(*replace_entry, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(replace_label), *replace_entry);
    gtk_grid_attach(GTK_GRID(grid), replace_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), *replace_entry, 1, 1, 1, 1);
}

} // namespace

ReplaceWindow::ReplaceWindow(GtkWindow* parent, SearchController& search,
                             ReplaceController& replace, Host host)
    : search_(search), replace_(replace), host_(std::move(host)),
      window_(GTK_WINDOW(gtk_window_new())) { // GTK owns the window; it is borrowed
    build(parent);
    connect_text_signals();
    connect_option_signals();
    connect_button_signals();
    connect_window_signals();
    connect_keys();
    replace_.set_on_job_progress([this]() { on_job_progress(); });
    replace_.set_on_job_done([this](const ReplaceJobResult& result) { on_job_done(result); });
}

ReplaceWindow::~ReplaceWindow() {
    replace_.set_on_job_progress(nullptr);
    replace_.set_on_job_done(nullptr);
    if (listening_) {
        search_.set_on_status_changed(nullptr);
    }
}

bool ReplaceWindow::is_open() const {
    return gtk_widget_get_visible(GTK_WIDGET(window_)) != FALSE;
}

void ReplaceWindow::present() {
    gtk_window_present(window_);
}

void ReplaceWindow::build(GtkWindow* parent) {
    gtk_window_set_title(window_, "Replace");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_window_set_default_size(window_, kWindowWidth, -1);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-replace");

    GtkWidget* grid = gtk_grid_new(); // floating; the window takes it over
    gtk_grid_set_row_spacing(GTK_GRID(grid), kRowSpacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), kColumnSpacing);
    gtk_widget_set_margin_top(grid, kMargin);
    gtk_widget_set_margin_bottom(grid, kMargin);
    gtk_widget_set_margin_start(grid, kMargin);
    gtk_widget_set_margin_end(grid, kMargin);

    build_entry_rows(grid, &find_entry_, &replace_entry_);

    find_next_button_ =
        gtk_button_new_with_mnemonic("Find _Next"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), find_next_button_, 2, 0, 1, 1);
    find_previous_button_ = gtk_button_new_with_mnemonic("Find Pre_vious");
    gtk_grid_attach(GTK_GRID(grid), find_previous_button_, 2, 1, 1, 1);
    replace_button_ = gtk_button_new_with_mnemonic("_Replace");
    gtk_grid_attach(GTK_GRID(grid), replace_button_, 2, 2, 1, 1);
    replace_all_button_ = gtk_button_new_with_mnemonic("Replace _All");
    gtk_grid_attach(GTK_GRID(grid), replace_all_button_, 2, 3, 1, 1);
    close_button_ = gtk_button_new_with_mnemonic("Cl_ose");
    gtk_grid_attach(GTK_GRID(grid), close_button_, 2, 4, 1, 1);

    gtk_window_set_default_widget(window_, find_next_button_);
    build_options(grid);
    build_scope(grid);
    gtk_window_set_child(window_, grid);
}

void ReplaceWindow::build_options(GtkWidget* grid) {
    match_case_ =
        gtk_check_button_new_with_mnemonic("Match _case"); // floating; the grid takes it over
    gtk_grid_attach(GTK_GRID(grid), match_case_, 0, 2, 2, 1);
    whole_word_ = gtk_check_button_new_with_mnemonic("Match _whole word only");
    gtk_grid_attach(GTK_GRID(grid), whole_word_, 0, 3, 2, 1);
    regex_ = gtk_check_button_new_with_mnemonic("Regular e_xpression");
    gtk_grid_attach(GTK_GRID(grid), regex_, 0, 4, 2, 1);
}

void ReplaceWindow::build_scope(GtkWidget* grid) {
    GtkWidget* scope_label = gtk_label_new("Replace All in:"); // floating; the grid takes it over
    gtk_label_set_xalign(GTK_LABEL(scope_label), 0.0f);
    gtk_grid_attach(GTK_GRID(grid), scope_label, 0, 5, 2, 1);

    whole_document_ = gtk_check_button_new_with_mnemonic("Whole _document");
    gtk_grid_attach(GTK_GRID(grid), whole_document_, 0, 6, 2, 1);
    selection_only_ = gtk_check_button_new_with_mnemonic("_Selection only");
    gtk_grid_attach(GTK_GRID(grid), selection_only_, 0, 7, 2, 1);
    gtk_check_button_set_group(GTK_CHECK_BUTTON(selection_only_),
                               GTK_CHECK_BUTTON(whole_document_));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(whole_document_), TRUE);

    status_ = gtk_label_new(""); // floating; the grid takes it over
    gtk_label_set_xalign(GTK_LABEL(status_), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(status_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(status_), kStatusWidthChars);
    gtk_widget_add_css_class(status_, "vm-find-status");
    gtk_grid_attach(GTK_GRID(grid), status_, 0, 8, 3, 1);
}

void ReplaceWindow::connect_text_signals() {
    signals_.push_back(gtkx::connect<void(GtkEditable*)>(
        find_entry_, "changed", [this](GtkEditable*) { on_find_text_changed(); }));
    signals_.push_back(gtkx::connect<void(GtkEditable*)>(
        replace_entry_, "changed", [this](GtkEditable*) { on_replacement_changed(); }));
    signals_.push_back(gtkx::connect<void(GtkEntry*)>(replace_entry_, "activate",
                                                      [this](GtkEntry*) { replace_once(); }));
}

void ReplaceWindow::connect_option_signals() {
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        match_case_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                search_.set_match_case(gtk_check_button_get_active(button) != FALSE);
                update_status();
            }
        }));
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        whole_word_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                search_.set_whole_word(gtk_check_button_get_active(button) != FALSE);
                update_status();
            }
        }));
    signals_.push_back(
        gtkx::connect<void(GtkCheckButton*)>(regex_, "toggled", [this](GtkCheckButton* button) {
            if (!syncing_) {
                search_.set_regex(gtk_check_button_get_active(button) != FALSE);
                update_status();
            }
        }));
    signals_.push_back(gtkx::connect<void(GtkCheckButton*)>(
        selection_only_, "toggled", [this](GtkCheckButton*) { on_scope_toggled(); }));
}

void ReplaceWindow::connect_button_signals() {
    signals_.push_back(gtkx::connect<void(GtkButton*)>(find_next_button_, "clicked",
                                                       [this](GtkButton*) { find_next(); }));
    signals_.push_back(gtkx::connect<void(GtkButton*)>(find_previous_button_, "clicked",
                                                       [this](GtkButton*) { find_previous(); }));
    signals_.push_back(gtkx::connect<void(GtkButton*)>(replace_button_, "clicked",
                                                       [this](GtkButton*) { replace_once(); }));
    signals_.push_back(gtkx::connect<void(GtkButton*)>(replace_all_button_, "clicked",
                                                       [this](GtkButton*) { replace_all(); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(close_button_, "clicked", [this](GtkButton*) { close(); }));
}

void ReplaceWindow::connect_window_signals() {
    signals_.push_back(gtkx::connect<gboolean(GtkWindow*)>(window_, "close-request",
                                                           [this](GtkWindow*) -> gboolean {
                                                               if (replace_.job_active()) {
                                                                   replace_.cancel_job();
                                                               } else {
                                                                   close();
                                                               }
                                                               return TRUE;
                                                           }));
    signals_.push_back(gtkx::connect<void(GObject*, GParamSpec*)>(
        window_, "notify::is-active", [this](GObject*, GParamSpec*) {
            if (gtk_window_is_active(window_)) {
                search_.refresh_status();
                update_status();
                update_scope();
            }
        }));
}

void ReplaceWindow::connect_keys() {
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
    gtk_widget_add_controller(find_entry_, entry_keys);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    signals_.push_back(
        gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
            keys, "key-pressed",
            [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType state) -> gboolean {
                if (keyval == GDK_KEY_Escape) {
                    if (replace_.job_active()) {
                        replace_.cancel_job();
                    } else {
                        close();
                    }
                    return TRUE;
                }
                if (replace_.job_active()) {
                    // Find, Find Next and Find Previous are off while a job runs.
                    const bool find_key =
                        keyval == GDK_KEY_F3 || ((keyval == GDK_KEY_f || keyval == GDK_KEY_F) &&
                                                 (state & GDK_CONTROL_MASK) != 0);
                    return find_key ? TRUE : FALSE;
                }
                if (keyval == GDK_KEY_F3 && (state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) == 0) {
                    if ((state & GDK_SHIFT_MASK) != 0) {
                        find_previous();
                    } else {
                        find_next();
                    }
                    return TRUE;
                }
                const bool control_f = (keyval == GDK_KEY_f || keyval == GDK_KEY_F) &&
                                       (state & GDK_CONTROL_MASK) != 0 &&
                                       (state & GDK_ALT_MASK) == 0;
                if (control_f && host_.show_find) {
                    host_.show_find();
                    return TRUE;
                }
                return FALSE;
            }));
    gtk_widget_add_controller(GTK_WIDGET(window_), keys);
}

void ReplaceWindow::on_find_text_changed() {
    if (syncing_) {
        return;
    }
    const char* text =
        gtk_editable_get_text(GTK_EDITABLE(find_entry_)); // borrowed; copied by set_text
    search_.set_text(text);
}

void ReplaceWindow::on_replacement_changed() {
    if (syncing_) {
        return;
    }
    const char* text = gtk_editable_get_text(GTK_EDITABLE(replace_entry_)); // borrowed; copied
    replace_.set_replacement(text);
    update_status();
}

void ReplaceWindow::on_scope_toggled() {
    if (syncing_) {
        return;
    }
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(selection_only_)) != FALSE) {
        if (!replace_.choose_scope(ReplaceScope::SelectionOnly)) {
            update_scope();
        }
    } else {
        replace_.choose_scope(ReplaceScope::WholeDocument);
    }
}

void ReplaceWindow::replace_once() {
    ReplaceResult result = replace_.replace();
    if (result.replaced || result.next.found) {
        scroll_to_match();
    }
    update_status();
    update_scope();
}

void ReplaceWindow::replace_all() {
    if (replace_.job_active()) {
        replace_.cancel_job();
        return;
    }
    if (replace_.start_replace_all() == ReplaceStart::Started) {
        // The host may end the job again at once, so the window's own state is set after it.
        if (host_.job_started) {
            host_.job_started();
        }
        if (replace_.job_active()) {
            show_job_running(true);
        }
    }
    update_status();
    update_scope();
}

void ReplaceWindow::on_job_progress() {
    update_status();
    if (host_.message) {
        host_.message(replace_.status());
    }
}

// Runs from the controller's done callback: after a Cancel, after the last slice, or from inside a
// change of tab. A tab change leaves the controller without a message of its own, so the main
// status bar is told how many matches were replaced.
void ReplaceWindow::on_job_done(const ReplaceJobResult& result) {
    show_job_running(false);
    update_status();
    if (host_.message) {
        std::string message = replace_.status();
        if (result.end == ReplaceJobEnd::BufferChanged) {
            message = replace_stopped_message(result.count);
            if (result.in_selection) {
                message += " in the selection";
            }
        }
        host_.message(message);
    }
    if (host_.job_ended) {
        host_.job_ended();
    }
}

void ReplaceWindow::show_job_running(bool running) {
    const gboolean usable = running ? FALSE : TRUE;
    for (GtkWidget* widget :
         {find_entry_, replace_entry_, match_case_, whole_word_, regex_, whole_document_,
          find_next_button_, find_previous_button_, replace_button_, close_button_}) {
        gtk_widget_set_sensitive(widget, usable);
    }
    gtk_button_set_label(GTK_BUTTON(replace_all_button_), running ? "C_ancel" : "Replace _All");
    update_scope(); // "Selection only" follows the selection again when the job is over
    gtk_widget_grab_focus(running ? replace_all_button_ : find_entry_);
}

void ReplaceWindow::find_next() {
    FindResult result = search_.find_next();
    if (result.found) {
        scroll_to_match();
    }
}

void ReplaceWindow::find_previous() {
    FindResult result = search_.find_previous();
    if (result.found) {
        scroll_to_match();
    }
}

void ReplaceWindow::sync_widgets(const std::string& text) {
    syncing_ = true;
    gtk_editable_set_text(GTK_EDITABLE(find_entry_), text.c_str());
    gtk_editable_set_text(GTK_EDITABLE(replace_entry_), replace_.replacement().c_str());
    const SearchOptions& options = search_.options();
    gtk_check_button_set_active(GTK_CHECK_BUTTON(match_case_), options.match_case ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(whole_word_), options.whole_word ? TRUE : FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(regex_), options.regex ? TRUE : FALSE);
    syncing_ = false;
    update_scope();
}

void ReplaceWindow::update_scope() {
    syncing_ = true;
    const bool usable = !replace_.job_active() && replace_.selection_available();
    gtk_widget_set_sensitive(selection_only_, usable ? TRUE : FALSE);
    if (replace_.scope() == ReplaceScope::SelectionOnly) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(selection_only_), TRUE);
    } else {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(whole_document_), TRUE);
    }
    syncing_ = false;
}

void ReplaceWindow::update_status() {
    gtk_label_set_text(GTK_LABEL(status_), replace_.status().c_str());
    if (replace_.status_is_problem()) {
        gtk_widget_add_css_class(status_, "vm-find-problem");
    } else {
        gtk_widget_remove_css_class(status_, "vm-find-problem");
    }
}

void ReplaceWindow::open(const std::optional<std::string>& selection) {
    replace_.capture_selection();
    if (selection.has_value()) {
        sync_widgets(*selection);
        search_.set_text(*selection);
        search_.apply();
    } else {
        sync_widgets(search_.text());
    }
    search_.set_on_status_changed([this] { update_status(); });
    listening_ = true;
    search_.refresh_status();
    update_status();
    update_scope();
    gtk_window_present(window_);
    gtk_widget_grab_focus(find_entry_);
    gtk_editable_select_region(GTK_EDITABLE(find_entry_), 0, -1);
}

void ReplaceWindow::close() {
    if (!is_open()) {
        return;
    }
    hide();
    if (host_.closed) {
        host_.closed();
    }
}

void ReplaceWindow::hide() {
    if (!is_open()) {
        return;
    }
    if (listening_) {
        search_.set_on_status_changed(nullptr);
        listening_ = false;
    }
    gtk_widget_set_visible(GTK_WIDGET(window_), FALSE);
}

void ReplaceWindow::scroll_to_match() {
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
