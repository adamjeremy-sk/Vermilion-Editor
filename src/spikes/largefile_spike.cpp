// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Throwaway spike: opens one large file in a GtkSourceView and prints load time and memory use.

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace {

// Command-line options, parsed before GTK starts.
bool g_no_highlight = false;
bool g_wrap = false;
bool g_language_after_load = false;
std::string g_path;

// Shared state for the async load callback; the window and buffer outlive the load.
GtkWindow* g_window = nullptr;
GtkTextBuffer* g_buffer = nullptr;
gint64 g_load_start_us = 0;
std::string g_file_name;

// VmRSS from /proc/self/status in kB, divided by 1024; 0 if the line is missing.
int read_rss_mb() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) != 0) {
            continue;
        }
        std::size_t pos = line.find_first_not_of(" \t", 6);
        if (pos == std::string::npos) {
            return 0;
        }
        long kb = 0;
        bool any = false;
        for (std::size_t i = pos; i < line.size(); ++i) {
            char c = line[i];
            if (c < '0' || c > '9') {
                break;
            }
            kb = kb * 10 + (c - '0');
            any = true;
        }
        if (!any) {
            return 0;
        }
        return static_cast<int>(kb / 1024);
    }
    return 0;
}

gboolean on_tick(gpointer user_data) {
    (void)user_data;
    g_print("SPIKE rss_mb=%d\n", read_rss_mb());
    fflush(stdout);
    return G_SOURCE_CONTINUE;
}

void on_load_done(GObject* source_object, GAsyncResult* res, gpointer user_data) {
    (void)user_data;
    auto* loader = GTK_SOURCE_FILE_LOADER(source_object);
    GError* raw_error = nullptr;
    gboolean ok = gtk_source_file_loader_load_finish(loader, res, &raw_error);
    std::unique_ptr<GError, decltype(&g_error_free)> error(raw_error, &g_error_free);
    if (!ok) {
        if (error) {
            g_printerr("%s\n", error->message);
        }
        if (g_window != nullptr) {
            gtk_window_set_title(g_window, "load failed");
        }
        return;
    }
    gint64 load_ms = (g_get_monotonic_time() - g_load_start_us) / 1000;
    glong lines = gtk_text_buffer_get_line_count(g_buffer);
    glong chars = gtk_text_buffer_get_char_count(g_buffer);
    g_print("SPIKE load_ms=%lld lines=%ld chars=%ld rss_mb=%d\n", static_cast<long long>(load_ms),
            static_cast<long>(lines), static_cast<long>(chars), read_rss_mb());
    fflush(stdout);
    if (g_language_after_load) {
        auto manager = vermilion::gtkx::ObjectPtr<GtkSourceLanguageManager>::ref(
            gtk_source_language_manager_get_default());
        auto language = vermilion::gtkx::ObjectPtr<GtkSourceLanguage>::ref(
            gtk_source_language_manager_guess_language(manager.get(), g_file_name.c_str(),
                                                       nullptr));
        const char* lang_id = language ? gtk_source_language_get_id(language.get()) : nullptr;
        gint64 set_start = g_get_monotonic_time();
        if (language) {
            gtk_source_buffer_set_language(GTK_SOURCE_BUFFER(g_buffer), language.get());
        }
        gint64 set_ms = (g_get_monotonic_time() - set_start) / 1000;
        g_print("SPIKE language_after_load=%s set_ms=%lld rss_mb=%d\n",
                lang_id != nullptr ? lang_id : "none", static_cast<long long>(set_ms),
                read_rss_mb());
        fflush(stdout);
    }
    std::string title = g_file_name + " - " + std::to_string(static_cast<long>(lines)) +
                        " lines - " + std::to_string(static_cast<long long>(load_ms)) + " ms";
    if (g_window != nullptr) {
        gtk_window_set_title(g_window, title.c_str());
    }
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(g_buffer, &start);
    gtk_text_buffer_place_cursor(g_buffer, &start);
}

void on_activate(GtkApplication* app) {
    g_print("SPIKE versions gtk=%u.%u.%u gtksourceview=%u.%u.%u\n", gtk_get_major_version(),
            gtk_get_minor_version(), gtk_get_micro_version(), gtk_source_get_major_version(),
            gtk_source_get_minor_version(), gtk_source_get_micro_version());
    fflush(stdout);

    g_window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_default_size(g_window, 1100, 700);

    auto buffer =
        vermilion::gtkx::ObjectPtr<GtkSourceBuffer>::adopt(gtk_source_buffer_new(nullptr));
    g_buffer = GTK_TEXT_BUFFER(buffer.get());

    auto view = vermilion::gtkx::ObjectPtr<GtkSourceView>::adopt_floating(
        GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(buffer.get())));
    gtk_source_view_set_show_line_numbers(view.get(), TRUE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view.get()), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view.get()),
                                g_wrap ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);

    auto scrolled = vermilion::gtkx::ObjectPtr<GtkScrolledWindow>::adopt_floating(
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new()));
    gtk_scrolled_window_set_child(scrolled.get(), GTK_WIDGET(view.get()));
    gtk_window_set_child(g_window, GTK_WIDGET(scrolled.get()));

    if (!g_language_after_load) {
        auto manager = vermilion::gtkx::ObjectPtr<GtkSourceLanguageManager>::ref(
            gtk_source_language_manager_get_default());
        auto language = vermilion::gtkx::ObjectPtr<GtkSourceLanguage>::ref(
            gtk_source_language_manager_guess_language(manager.get(), g_file_name.c_str(),
                                                       nullptr));
        if (language) {
            gtk_source_buffer_set_language(buffer.get(), language.get());
        }
        const char* lang_id = language ? gtk_source_language_get_id(language.get()) : nullptr;
        g_print("SPIKE language=%s\n", lang_id != nullptr ? lang_id : "none");
        fflush(stdout);
    }
    if (g_no_highlight) {
        gtk_source_buffer_set_highlight_syntax(buffer.get(), FALSE);
    }

    auto file = vermilion::gtkx::ObjectPtr<GtkSourceFile>::adopt(gtk_source_file_new());
    auto location = vermilion::gtkx::ObjectPtr<GFile>::adopt(g_file_new_for_path(g_path.c_str()));
    gtk_source_file_set_location(file.get(), location.get());

    // Keep the loader alive for the whole async load by parking it on the buffer.
    auto loader = vermilion::gtkx::ObjectPtr<GtkSourceFileLoader>::adopt(
        gtk_source_file_loader_new(buffer.get(), file.get()));
    GtkSourceFileLoader* loader_keep = loader.get();
    g_object_set_data_full(G_OBJECT(buffer.get()), "spike-loader", loader.release(),
                           g_object_unref);

    g_load_start_us = g_get_monotonic_time();
    gtk_source_file_loader_load_async(loader_keep, G_PRIORITY_DEFAULT, nullptr, nullptr, nullptr,
                                      nullptr, on_load_done, nullptr);

    g_timeout_add_seconds(2, on_tick, nullptr);
    gtk_window_present(g_window);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        g_printerr("usage: vermilion-spike-largefile <path> [--no-highlight] [--wrap] "
                   "[--language-after-load]\n");
        return 2;
    }
    g_path = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-highlight") {
            g_no_highlight = true;
        } else if (arg == "--wrap") {
            g_wrap = true;
        } else if (arg == "--language-after-load") {
            g_language_after_load = true;
        }
    }
    g_file_name = g_path;
    std::size_t slash = g_path.find_last_of('/');
    if (slash != std::string::npos) {
        g_file_name = g_path.substr(slash + 1);
    }

    auto app = vermilion::gtkx::ObjectPtr<GtkApplication>::adopt(
        gtk_application_new("io.github.adamjeremy_sk.VermilionSpike", G_APPLICATION_NON_UNIQUE));
    vermilion::gtkx::SignalConnection activate = vermilion::gtkx::connect<void(GtkApplication*)>(
        app.get(), "activate", [](GtkApplication* application) { on_activate(application); });
    return g_application_run(G_APPLICATION(app.get()), 1, argv);
}
