// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the main window, its action table and shortcuts, and the questions before closing.

#include "ui/main_window.h"

#include <cstddef>
#include <optional>
#include <string>

#include "core/version.h"
#include "ui/close_dialog.h"
#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

namespace {

constexpr const char* kMenuResource = "/io/github/adamjeremy_sk/Vermilion/menus.ui";

// Commands that need an open document; greyed out in the blank window.
constexpr const char* kDocumentActions[] = {
    "close-tab", "undo", "redo", "cut", "copy", "paste", "select-all", "next-tab", "previous-tab"};

MainWindow* self(gpointer data) {
    return static_cast<MainWindow*>(data);
}

void on_new_file(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->new_file();
}
void on_close_tab(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->close_active();
}
void on_quit(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->quit();
}
void on_undo(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("text.undo");
}
void on_redo(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("text.redo");
}
void on_cut(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("clipboard.cut");
}
void on_copy(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("clipboard.copy");
}
void on_paste(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("clipboard.paste");
}
void on_select_all(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->edit("selection.select-all");
}
void on_next_tab(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->next_tab();
}
void on_previous_tab(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->previous_tab();
}
void on_about(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->about();
}

const GActionEntry kActions[] = {
    {"new-file", on_new_file, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"close-tab", on_close_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"quit", on_quit, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"undo", on_undo, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"redo", on_redo, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"cut", on_cut, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"copy", on_copy, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"paste", on_paste, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"select-all", on_select_all, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"next-tab", on_next_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"previous-tab", on_previous_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"about", on_about, nullptr, nullptr, nullptr, {0, 0, 0}},
};

struct Shortcut {
    const char* action;
    const char* accels[3];
};

const Shortcut kShortcuts[] = {
    {"win.new-file", {"<Control>n", nullptr, nullptr}},
    {"win.close-tab", {"<Control>w", nullptr, nullptr}},
    {"win.quit", {"<Control>q", nullptr, nullptr}},
    {"win.undo", {"<Control>z", nullptr, nullptr}},
    {"win.redo", {"<Control>y", "<Control><Shift>z", nullptr}},
    {"win.cut", {"<Control>x", nullptr, nullptr}},
    {"win.copy", {"<Control>c", nullptr, nullptr}},
    {"win.paste", {"<Control>v", nullptr, nullptr}},
    {"win.select-all", {"<Control>a", nullptr, nullptr}},
    {"win.next-tab", {"<Control>Page_Down", nullptr, nullptr}},
    {"win.previous-tab", {"<Control>Page_Up", nullptr, nullptr}},
};

} // namespace

void MainWindow::install_menus_and_shortcuts(GtkApplication* app) {
    // The builder owns the menu model; the application takes its own reference.
    auto builder = gtkx::ObjectPtr<GtkBuilder>::adopt(gtk_builder_new_from_resource(kMenuResource));
    GObject* menubar = gtk_builder_get_object(builder.get(), "menubar"); // borrowed
    if (menubar != nullptr) {
        gtk_application_set_menubar(app, G_MENU_MODEL(menubar));
    }
    for (const Shortcut& shortcut : kShortcuts) {
        gtk_application_set_accels_for_action(app, shortcut.action, shortcut.accels);
    }
}

// The window from _new is owned by GTK (its toplevel list and the application), so it is
// borrowed here, not adopted.
MainWindow::MainWindow(GtkApplication* app)
    : window_(GTK_WINDOW(gtk_application_window_new(app))),
      tabs_(list_, DocumentTabs::Callbacks{[this] { refresh(); },
                                           [this](DocumentId id) { request_close(id); }}) {
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_window_set_title(window_, "Vermilion Editor");
    gtk_window_set_default_size(window_, 1100, 700);
    g_action_map_add_action_entries(G_ACTION_MAP(window_), kActions, G_N_ELEMENTS(kActions), this);
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window_), TRUE);
    // Floating box from _new; the window sinks it. The window box adds its own reference to
    // the notebook and the status bar, which tabs_ and status_ also keep.
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(root), tabs_.widget());
    gtk_box_append(GTK_BOX(root), status_.widget());
    gtk_window_set_child(window_, root);
    close_request_ = gtkx::connect<gboolean(GtkWindow*)>(
        window_, "close-request",
        [this](GtkWindow*) -> gboolean { return on_close_request() ? TRUE : FALSE; });
    tabs_.add_untitled();
    refresh();
}

void MainWindow::present() {
    gtk_window_present(window_);
}

void MainWindow::new_file() {
    tabs_.add_untitled();
}

void MainWindow::close_active() {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        request_close(*active);
    }
}

// Goes through "close-request", so unsaved changes are asked about the same way as the
// window's own close button.
void MainWindow::quit() {
    gtk_window_close(window_);
}

// An open menu holds the keyboard focus, so the command goes to the active editor, not to the
// focused widget.
void MainWindow::edit(const char* widget_action) {
    GtkWidget* view = tabs_.active_view(); // borrowed
    if (view != nullptr) {
        gtk_widget_activate_action(view, widget_action, nullptr);
    }
}

void MainWindow::next_tab() {
    tabs_.select_next();
}

void MainWindow::previous_tab() {
    tabs_.select_previous();
}

void MainWindow::about() {
    // The icon is named after the application ID (borrowed strings).
    GtkApplication* app = gtk_window_get_application(window_);
    const char* icon = g_application_get_application_id(G_APPLICATION(app));
    gtk_show_about_dialog(
        window_, "program-name", "Vermilion Editor", "logo-icon-name", icon, "version",
        vermilion::version(), "comments", "A native Linux text and code editor.", "copyright",
        "Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)", "license-type", GTK_LICENSE_GPL_3_0,
        "website", "https://github.com/adamjeremy-sk/Vermilion-Editor", nullptr);
}

void MainWindow::request_close(DocumentId id) {
    const DocumentRecord* record = list_.find(id);
    if (record == nullptr) {
        return;
    }
    if (!record->modified) {
        tabs_.close(id);
        return;
    }
    // Saving arrives with M2.2, which adds a Save button to this question.
    ask_discard(window_, "Close \u201c" + record->title + "\u201d without saving?",
                "Its changes will be lost.", "Close Without Saving",
                [this, id] { tabs_.close(id); });
}

bool MainWindow::on_close_request() {
    std::size_t count = list_.modified_count();
    if (quit_confirmed_ || count == 0) {
        return false;
    }
    std::string message = count == 1
                              ? std::string("Quit without saving 1 document?")
                              : "Quit without saving " + std::to_string(count) + " documents?";
    ask_discard(window_, message, "Their changes will be lost.", "Quit Without Saving", [this] {
        quit_confirmed_ = true;
        gtk_window_close(window_);
    });
    return true;
}

void MainWindow::refresh() {
    bool has_document = !list_.empty();
    for (const char* action : kDocumentActions) {
        set_enabled(action, has_document);
    }
    std::optional<CursorStatus> status = tabs_.active_status();
    if (status) {
        status_.show_document(*status);
    } else {
        status_.show_no_document();
    }
}

void MainWindow::set_enabled(const char* name, bool enabled) {
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(window_), name); // borrowed
    if (action != nullptr) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled ? TRUE : FALSE);
    }
}

} // namespace vermilion::ui
