// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Program entry point: creates the GtkApplication and opens the main window on activate.

#include <gtk/gtk.h>

#include <string>

#include "core/version.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace {

void on_activate(GtkApplication* app) {
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
    vermilion::gtkx::ObjectPtr<GtkApplication> app =
        vermilion::gtkx::ObjectPtr<GtkApplication>::adopt(
            gtk_application_new("io.github.adamjeremy_sk.Vermilion", G_APPLICATION_DEFAULT_FLAGS));
    vermilion::gtkx::SignalConnection activate = vermilion::gtkx::connect<void(GtkApplication*)>(
        app.get(), "activate", [](GtkApplication* application) { on_activate(application); });
    return g_application_run(G_APPLICATION(app.get()), argc, argv);
}
