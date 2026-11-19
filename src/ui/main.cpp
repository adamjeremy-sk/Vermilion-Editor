// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Program entry point: registers the resources, creates the GtkApplication, and answers the
// activation and the command line of each launch.

#include <gtk/gtk.h>

#include <cstring>
#include <memory>

#include "ui/command_line.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"
#include "ui/main_window.h"
#include "ui/style.h"
#include "vermilion_manual.h"
#include "vermilion_resources.h"

namespace {

struct AppState {
    std::unique_ptr<vermilion::ui::MainWindow> window;
    vermilion::gtkx::SignalConnection window_destroy;
    std::unique_ptr<vermilion::ui::CommandLineHandler> command_line;
};

// Brings the window forward; the first call creates it.
vermilion::ui::MainWindow& show_window(GtkApplication* app, AppState& state) {
    if (state.window) {
        state.window->present();
        return *state.window;
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
    // The question about the recovery copies of an earlier run comes once the window is shown.
    state.window->offer_recovery();
    return *state.window;
}

void on_activate(GtkApplication* app, AppState& state) {
    show_window(app, state);
}

// A launch without files is an activation; a launch with files goes to the command-line handler,
// which calls show_window() when its questions are answered. The program takes no options of its
// own, so GApplication does not parse them: --help and -h are answered here (a file of that name is
// written ./--help).
int on_command_line(GApplication* app, GApplicationCommandLine* cmdline, AppState& state) {
    int argc = 0;
    gchar** argv = g_application_command_line_get_arguments(cmdline, &argc);
    bool help = false;
    for (int i = 1; i < argc; i++) {
        help = help || std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0;
    }
    g_strfreev(argv);
    if (help) {
        g_application_command_line_print(
            cmdline, "Usage:\n  vermilion [FILE...]\n\nOpens the files in tabs. A name that does "
                     "not exist is created after a question.\n");
        return 0;
    }
    if (argc <= 1) {
        g_application_activate(app);
        return 0;
    }
    state.command_line->handle(cmdline);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    vermilion_register_resource();
    vermilion_manual_register_resource();
    AppState state;
    vermilion::gtkx::ObjectPtr<GtkApplication> app =
        vermilion::gtkx::ObjectPtr<GtkApplication>::adopt(gtk_application_new(
            "io.github.adamjeremy_sk.Vermilion", G_APPLICATION_HANDLES_COMMAND_LINE));
    g_application_set_option_context_parameter_string(G_APPLICATION(app.get()), "[FILE...]");
    GtkApplication* application = app.get();
    state.command_line = std::make_unique<vermilion::ui::CommandLineHandler>(
        G_APPLICATION(application),
        [application, &state](const vermilion::ui::CommandLineHandler::Result& result) {
            show_window(application, state).open_command_line(result.paths, result.messages);
        });
    vermilion::gtkx::SignalConnection activate = vermilion::gtkx::connect<void(GtkApplication*)>(
        application, "activate", [&state](GtkApplication* active) { on_activate(active, state); });
    vermilion::gtkx::SignalConnection command_line =
        vermilion::gtkx::connect<int(GApplication*, GApplicationCommandLine*)>(
            application, "command-line",
            [&state](GApplication* launched, GApplicationCommandLine* cmdline) {
                return on_command_line(launched, cmdline, state);
            });
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    // Its launches hold the application; they let go while the application still exists.
    state.command_line.reset();
    return status;
}
