// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include <gtk/gtk.h>

#include <string>

#include "core/version.h"

namespace {

void on_activate(GtkApplication* app, gpointer user_data) {
    (void)user_data;
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Vermilion Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 700);

    std::string text = std::string("Vermilion Editor ") + vermilion::version();
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_window_set_child(GTK_WINDOW(window), label);

    gtk_window_present(GTK_WINDOW(window));
}

}  // namespace

int main(int argc, char** argv) {
    GtkApplication* app =
        gtk_application_new("io.github.adamjeremy_sk.Vermilion", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
