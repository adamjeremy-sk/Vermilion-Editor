// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Program entry point: registers the resources, creates the GtkApplication and the main window.

#include <gtk/gtk.h>

#include <memory>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/main_window.h"
#include "ui/style.h"
#include "vermilion_resources.h"

namespace {

struct AppState {
    std::unique_ptr<vermilion::ui::MainWindow> window;
    vermilion::gtkx::SignalConnection window_destroy;
};

void on_activate(GtkApplication* app, AppState& state) {
    if (state.window) {
        state.window->present();
        return;
    }
    vermilion::ui::install_classic_style();
    // The icon is named after the application ID; GTK picks the size (badge in the title bar).
    gtk_window_set_default_icon_name(g_application_get_application_id(G_APPLICATION(app)));
    vermilion::ui::MainWindow::install_menus_and_shortcuts(app);
    state.window = std::make_unique<vermilion::ui::MainWindow>(app);
    // The C++ side of the window goes when GTK destroys the window.
    state.window_destroy = vermilion::gtkx::connect<void(GtkWidget*)>(
        state.window->window(), "destroy", [&state](GtkWidget*) { state.window.reset(); });
    state.window->present();
}

} // namespace

int main(int argc, char** argv) {
    vermilion_register_resource();
    AppState state;
    vermilion::gtkx::ObjectPtr<GtkApplication> app =
        vermilion::gtkx::ObjectPtr<GtkApplication>::adopt(
            gtk_application_new("io.github.adamjeremy_sk.Vermilion", G_APPLICATION_DEFAULT_FLAGS));
    vermilion::gtkx::SignalConnection activate = vermilion::gtkx::connect<void(GtkApplication*)>(
        app.get(), "activate",
        [&state](GtkApplication* application) { on_activate(application, state); });
    return g_application_run(G_APPLICATION(app.get()), argc, argv);
}
