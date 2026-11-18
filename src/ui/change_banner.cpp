// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The change banner: a label and two buttons in a box, shown above a document's text.

#include "ui/change_banner.h"

#include <utility>

namespace vermilion::ui {

ChangeBanner::ChangeBanner(Callbacks callbacks) : callbacks_(std::move(callbacks)) {
    box_ = gtkx::ObjectPtr<GtkWidget>::adopt_floating(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
    gtk_widget_add_css_class(box_.get(), "vm-change-banner");
    label_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label_), TRUE);
    gtk_widget_set_hexpand(label_, TRUE);
    primary_ = gtk_button_new_with_label("");
    secondary_ = gtk_button_new_with_label("");
    gtk_widget_set_valign(primary_, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(secondary_, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box_.get()), label_);
    gtk_box_append(GTK_BOX(box_.get()), primary_);
    gtk_box_append(GTK_BOX(box_.get()), secondary_);
    gtk_widget_set_visible(box_.get(), FALSE);
    primary_clicked_ = gtkx::connect<void(GtkButton*)>(primary_, "clicked", [this](GtkButton*) {
        std::function<void()> run = callbacks_.primary;
        if (run) {
            run();
        }
    });
    secondary_clicked_ = gtkx::connect<void(GtkButton*)>(secondary_, "clicked", [this](GtkButton*) {
        std::function<void()> run = callbacks_.secondary;
        if (run) {
            run();
        }
    });
}

void ChangeBanner::show(const std::string& text, const std::string& primary_label,
                        const std::string& secondary_label) {
    text_ = text;
    gtk_label_set_text(GTK_LABEL(label_), text.c_str());
    gtk_button_set_label(GTK_BUTTON(primary_), primary_label.c_str());
    gtk_button_set_label(GTK_BUTTON(secondary_), secondary_label.c_str());
    gtk_widget_set_visible(box_.get(), TRUE);
}

void ChangeBanner::hide() {
    gtk_widget_set_visible(box_.get(), FALSE);
    text_.clear();
    gtk_label_set_text(GTK_LABEL(label_), "");
}

bool ChangeBanner::visible() const {
    return gtk_widget_get_visible(box_.get()) != FALSE;
}

} // namespace vermilion::ui
