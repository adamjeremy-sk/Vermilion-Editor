// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: classic stylesheet over a chosen base theme, and the desktop dark-mode signals.

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "spikes/look_style.h"
#include "spikes/look_widgets.h"
#include "ui/gtkx/string.h"

namespace {

using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::SignalConnection;
using vermilion::spike::apply;
using vermilion::spike::LookState;
using vermilion::spike::Mode;
using vermilion::spike::PortalScheme;
using vermilion::spike::read_prefer_dark;
using vermilion::spike::read_theme_name;

using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

void on_noop(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    (void)user_data;
}

void on_quit(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    auto* s = static_cast<LookState*>(user_data);
    g_application_quit(G_APPLICATION(s->app));
}

void on_reload_css(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    apply(*static_cast<LookState*>(user_data), "reload");
}

void on_message_dialog(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    (void)action;
    (void)parameter;
    auto* s = static_cast<LookState*>(user_data);
    if (s->window == nullptr) {
        return;
    }
    // Full reference from _new (not floating); show() keeps its own while the dialog is open.
    auto dialog = ObjectPtr<GtkAlertDialog>::adopt(gtk_alert_dialog_new("%s", "Native dialog"));
    gtk_alert_dialog_set_detail(dialog.get(), "Dialogs keep the base theme's look.");
    gtk_alert_dialog_show(dialog.get(), s->window);
}

void on_color_mode(GSimpleAction* action, GVariant* value, gpointer user_data) {
    auto* s = static_cast<LookState*>(user_data);
    const char* target = g_variant_get_string(value, nullptr);
    if (std::strcmp(target, "light") == 0) {
        s->mode = Mode::Light;
    } else if (std::strcmp(target, "dark") == 0) {
        s->mode = Mode::Dark;
    } else if (std::strcmp(target, "follow") == 0) {
        s->mode = Mode::Follow;
    } else {
        return;
    }
    g_simple_action_set_state(action, value);
    apply(*s, "colour-mode");
}

void on_base_theme(GSimpleAction* action, GVariant* value, gpointer user_data) {
    auto* s = static_cast<LookState*>(user_data);
    const char* target = g_variant_get_string(value, nullptr);
    if (std::strcmp(target, "desktop") == 0) {
        s->base_default = false;
    } else if (std::strcmp(target, "default") == 0) {
        s->base_default = true;
    } else {
        return;
    }
    g_simple_action_set_state(action, value);
    apply(*s, "base-theme");
}

const GActionEntry kActions[] = {
    {"noop", on_noop, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"quit", on_quit, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"reload-css", on_reload_css, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"message-dialog", on_message_dialog, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"color-mode", nullptr, "s", "'follow'", on_color_mode, {0, 0, 0}},
    {"base-theme", nullptr, "s", "'desktop'", on_base_theme, {0, 0, 0}},
};

void print_startup(GtkSettings* settings) {
    const char* desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char* gtk_theme_env = g_getenv("GTK_THEME");
    g_print("LOOK versions gtk=%u.%u.%u gtksourceview=%u.%u.%u\n", gtk_get_major_version(),
            gtk_get_minor_version(), gtk_get_micro_version(), gtk_source_get_major_version(),
            gtk_source_get_minor_version(), gtk_source_get_micro_version());
    g_print("LOOK env XDG_CURRENT_DESKTOP=%s GTK_THEME=%s\n", desktop != nullptr ? desktop : "",
            gtk_theme_env != nullptr ? gtk_theme_env : "");
    g_print("LOOK startup theme=%s prefer_dark=%d\n", read_theme_name(settings).c_str(),
            read_prefer_dark(settings) ? 1 : 0);
    GtkSourceStyleSchemeManager* manager = gtk_source_style_scheme_manager_get_default();
    std::string ids;
    for (const char* const* id = gtk_source_style_scheme_manager_get_scheme_ids(manager);
         id != nullptr && *id != nullptr; ++id) {
        ids += ids.empty() ? "" : ",";
        ids += *id;
    }
    g_print("LOOK schemes %s\n", ids.c_str());
    fflush(stdout);
}

GtkWidget* build_window(GtkApplication* app, LookState& s) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "vm-main");
    gtk_window_set_title(GTK_WINDOW(window), "Vermilion look spike");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 650);
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), TRUE);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), vermilion::spike::build_documents(s.buffers));
    gtk_paned_set_end_child(GTK_PANED(paned), vermilion::spike::build_side_panel());
    gtk_paned_set_position(GTK_PANED(paned), 720);
    gtk_box_append(GTK_BOX(root), paned);
    gtk_box_append(GTK_BOX(root), vermilion::spike::build_status_bar(&s.message));
    gtk_window_set_child(GTK_WINDOW(window), root);
    return window;
}

void install_css_provider(LookState& s) {
    s.provider = ObjectPtr<GtkCssProvider>::adopt(gtk_css_provider_new());
    s.css_error = vermilion::gtkx::connect<void(GtkCssProvider*, GtkCssSection*, GError*)>(
        s.provider.get(), "parsing-error",
        [](GtkCssProvider*, GtkCssSection* section, GError* error) {
            std::string where = vermilion::gtkx::take_string(gtk_css_section_to_string(section));
            g_print("LOOK css-error %s: %s\n", where.c_str(),
                    error != nullptr ? error->message : "unknown");
            fflush(stdout);
        });
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(s.provider.get()),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void install_menubar(GtkApplication* app, const LookState& s) {
    // The builder owns the menu model; the application takes its own reference.
    auto builder = ObjectPtr<GtkBuilder>::adopt(gtk_builder_new());
    std::string menu_path =
        vermilion::gtkx::take_string(g_build_filename(s.data_dir.c_str(), "look_menu.ui", nullptr));
    GError* raw_error = nullptr;
    if (gtk_builder_add_from_file(builder.get(), menu_path.c_str(), &raw_error)) {
        gtk_application_set_menubar(app,
                                    G_MENU_MODEL(gtk_builder_get_object(builder.get(), "menubar")));
    } else {
        ErrorPtr error(raw_error, &g_error_free);
        g_printerr("LOOK menu-load-failed %s: %s\n", menu_path.c_str(),
                   error ? error->message : "unknown error");
    }
}

// Every signal that can change the "Follow system" answer re-applies the look.
void watch_signals(GtkSettings* settings, LookState& s) {
    s.theme_notify = vermilion::gtkx::connect<void(GObject*, GParamSpec*)>(
        settings, "notify::gtk-theme-name", [&s](GObject*, GParamSpec*) {
            if (!s.applying) {
                apply(s, "theme-name-changed");
            }
        });
    s.prefer_notify = vermilion::gtkx::connect<void(GObject*, GParamSpec*)>(
        settings, "notify::gtk-application-prefer-dark-theme", [&s](GObject*, GParamSpec*) {
            if (!s.applying) {
                apply(s, "prefer-dark-changed");
            }
        });
    s.portal = std::make_unique<vermilion::spike::PortalWatcher>(
        [&s](PortalScheme scheme, const std::string& note) {
            s.portal_scheme = scheme;
            g_print("LOOK portal %s (%s)\n", vermilion::spike::portal_scheme_name(scheme),
                    note.c_str());
            apply(s, "portal");
        });
    s.portal->start();
}

void on_activate(GtkApplication* app, LookState& s) {
    GtkSettings* settings = gtk_settings_get_default();
    print_startup(settings);

    const char* const reload_accels[] = {"<Control>r", nullptr};
    const char* const quit_accels[] = {"<Control>q", nullptr};
    gtk_application_set_accels_for_action(app, "app.reload-css", reload_accels);
    gtk_application_set_accels_for_action(app, "app.quit", quit_accels);

    install_css_provider(s);
    install_menubar(app, s);

    GtkWidget* window = build_window(app, s);
    s.window = GTK_WINDOW(window);
    s.window_destroy =
        vermilion::gtkx::connect<void(GtkWidget*)>(window, "destroy", [&s](GtkWidget*) {
            s.window = nullptr;
            s.message = nullptr;
            s.buffers.clear();
        });

    watch_signals(settings, s);
    apply(s, "startup");
    gtk_window_present(s.window);
}

} // namespace

int main(int argc, char** argv) {
    LookState state;
    state.data_dir = argc > 1 ? argv[1] : VERMILION_SPIKE_DATA_DIR;
    auto app = ObjectPtr<GtkApplication>::adopt(gtk_application_new(
        "io.github.adamjeremy_sk.VermilionLookSpike", G_APPLICATION_NON_UNIQUE));
    state.app = app.get();
    g_action_map_add_action_entries(G_ACTION_MAP(app.get()), kActions, G_N_ELEMENTS(kActions),
                                    &state);
    SignalConnection activate = vermilion::gtkx::connect<void(GtkApplication*)>(
        app.get(), "activate",
        [&state](GtkApplication* application) { on_activate(application, state); });
    return g_application_run(G_APPLICATION(app.get()), 1, argv);
}
