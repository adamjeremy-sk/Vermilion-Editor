// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the Settings window and keeps the working copy of the settings.

#include "ui/settings_window.h"

#include <gtk/gtk.h>

#include <utility>

namespace vermilion::ui {
namespace {

constexpr int kMargin = 10;
constexpr int kButtonSpacing = 6;

} // namespace

SettingsWindow::SettingsWindow(GtkWindow* parent, Host host)
    : host_(std::move(host)), window_(GTK_WINDOW(gtk_window_new())) { // GTK owns the window
    build(parent);
    connect_signals();
    connect_keys();
}

bool SettingsWindow::is_open() const {
    return gtk_widget_get_visible(GTK_WIDGET(window_)) != FALSE;
}

void SettingsWindow::build(GtkWindow* parent) {
    gtk_window_set_title(window_, "Settings");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_window_set_modal(window_, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-settings");

    GtkWidget* column =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, kMargin); // floating; the window takes it over
    gtk_widget_set_margin_top(column, kMargin);
    gtk_widget_set_margin_bottom(column, kMargin);
    gtk_widget_set_margin_start(column, kMargin);
    gtk_widget_set_margin_end(column, kMargin);

    notebook_ = gtk_notebook_new(); // floating; the box takes it over
    gtk_widget_set_vexpand(notebook_, TRUE);
    gtk_widget_set_hexpand(notebook_, TRUE);
    gtk_box_append(GTK_BOX(column), notebook_);

    GtkWidget* buttons =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kButtonSpacing); // floating; the box takes it over
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    ok_ = gtk_button_new_with_mnemonic("_OK");         // floating; the box takes it over
    cancel_ = gtk_button_new_with_mnemonic("_Cancel"); // floating; the box takes it over
    apply_ = gtk_button_new_with_mnemonic("_Apply");   // floating; the box takes it over
    gtk_box_append(GTK_BOX(buttons), ok_);
    gtk_box_append(GTK_BOX(buttons), cancel_);
    gtk_box_append(GTK_BOX(buttons), apply_);
    gtk_widget_set_sensitive(apply_, FALSE);
    gtk_box_append(GTK_BOX(column), buttons);

    gtk_window_set_default_widget(window_, ok_);
    gtk_window_set_child(window_, column);
}

void SettingsWindow::connect_signals() {
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(ok_, "clicked", [this](GtkButton*) { ok(); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(cancel_, "clicked", [this](GtkButton*) { cancel(); }));
    signals_.push_back(
        gtkx::connect<void(GtkButton*)>(apply_, "clicked", [this](GtkButton*) { apply(); }));
    signals_.push_back(gtkx::connect<gboolean(GtkWindow*)>(window_, "close-request",
                                                           [this](GtkWindow*) -> gboolean {
                                                               cancel();
                                                               return TRUE;
                                                           }));
}

void SettingsWindow::connect_keys() {
    GtkEventController* window_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(window_keys, GTK_PHASE_CAPTURE);
    signals_.push_back(
        gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
            window_keys, "key-pressed",
            [this](GtkEventControllerKey*, guint keyval, guint, GdkModifierType) -> gboolean {
                if (keyval == GDK_KEY_Escape) {
                    cancel();
                    return TRUE;
                }
                return FALSE;
            }));
    gtk_widget_add_controller(GTK_WIDGET(window_), window_keys);
}

void SettingsWindow::add_page(const std::string& title, std::unique_ptr<SettingsPage> page) {
    page->set_changed([this] { on_page_changed(); });
    GtkWidget* label = gtk_label_new(title.c_str()); // floating; the notebook takes it over
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), page->widget(), label);
    pages_.push_back(std::move(page));
}

int SettingsWindow::page_count() const {
    return static_cast<int>(pages_.size());
}

void SettingsWindow::open() {
    if (is_open()) {
        gtk_window_present(window_);
        return;
    }
    applied_ = normalise_settings(host_.current ? host_.current() : default_settings());
    working_ = applied_;
    for (auto& page : pages_) {
        page->load(applied_);
    }
    gtk_widget_set_sensitive(apply_, FALSE);
    gtk_window_present(window_);
}

void SettingsWindow::on_page_changed() {
    EditorSettings next = applied_;
    for (auto& page : pages_) {
        page->store(next);
    }
    working_ = normalise_settings(std::move(next));
    gtk_widget_set_sensitive(apply_, working_ != applied_);
}

void SettingsWindow::ok() {
    apply();
    close();
}

void SettingsWindow::apply() {
    if (working_ == applied_) {
        return;
    }
    const EditorSettings chosen = working_;
    applied_ = chosen;
    gtk_widget_set_sensitive(apply_, FALSE);
    if (host_.apply) {
        host_.apply(chosen);
    }
}

void SettingsWindow::cancel() {
    close();
}

void SettingsWindow::close() {
    if (!is_open()) {
        return;
    }
    gtk_widget_set_visible(GTK_WIDGET(window_), FALSE);
    if (host_.closed) {
        host_.closed();
    }
}

} // namespace vermilion::ui
