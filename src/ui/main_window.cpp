// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the main window, wires its parts together and keeps the actions' enabled state current.

#include "ui/main_window.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/version.h"
#include "ui/editor_page.h"
#include "ui/gtkx/object_ptr.h"
#include "ui/window_actions.h"

namespace vermilion::ui {

namespace {

constexpr const char* kMenuResource = "/io/github/adamjeremy_sk/Vermilion/menus.ui";

// Commands that need an open document; greyed out in the blank window.
constexpr const char* kDocumentActions[] = {"close-tab",    "undo",     "redo",       "cut",
                                            "copy",         "paste",    "select-all", "next-tab",
                                            "previous-tab", "word-wrap"};

// Commands that write the document or change its format: also greyed out while it loads or
// saves, and for a read-only document. Reopen as needs a file and an idle document.
constexpr const char* kWriteActions[] = {"save", "save-as", "save-encoding", "line-ending"};

} // namespace

void MainWindow::install_menus_and_shortcuts(GtkApplication* app) {
    // The builder owns the menu model; the application takes its own reference.
    auto builder = gtkx::ObjectPtr<GtkBuilder>::adopt(gtk_builder_new_from_resource(kMenuResource));
    GObject* menubar = gtk_builder_get_object(builder.get(), "menubar"); // borrowed
    if (menubar != nullptr) {
        gtk_application_set_menubar(app, G_MENU_MODEL(menubar));
    }
    set_shortcuts(app);
}

// The window from _new is owned by GTK (its toplevel list and the application), so it is
// borrowed here, not adopted.
MainWindow::MainWindow(GtkApplication* app)
    : window_(GTK_WINDOW(gtk_application_window_new(app))),
      tabs_(list_, DocumentTabs::Callbacks{
                       [this] { refresh(); }, [this](DocumentId id) { saves_.request_close(id); },
                       [this](const std::string& message) { status_.show_message(message); },
                       [this](const std::string& message, const std::string& detail) {
                           show_error(message, detail);
                       },
                       [this](DocumentId id, DocumentSaver::Outcome outcome,
                              const std::string& error) { saves_.on_saved(id, outcome, error); }}),
      opener_(window_, FileOpener::Callbacks{
                           [this](const std::string& message) { status_.show_message(message); },
                           [this](const std::string& message, const std::string& detail) {
                               show_error(message, detail);
                           },
                           [this](const std::string& path) {
                               const DocumentRecord* open = list_.find_by_path(path);
                               if (open == nullptr) {
                                   return false;
                               }
                               tabs_.select(open->id);
                               return true;
                           },
                           [this](const std::string& path, const std::string& title,
                                  const ScanResult& scan) { tabs_.add_file(path, title, scan); }}),
      saves_(window_, list_, tabs_,
             SaveController::Callbacks{
                 [this] { refresh(); },
                 [this](const std::string& message) { status_.show_message(message); },
                 [this](const std::string& message, const std::string& detail) {
                     show_error(message, detail);
                 }}),
      formats_(window_, list_, tabs_,
               FormatCommands::Callbacks{
                   [this] { refresh(); },
                   [this](const std::string& message) { status_.show_message(message); },
                   [this](DocumentId id, std::function<void(bool)> done) {
                       saves_.save(id, std::move(done));
                   }}) {
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_window_set_title(window_, "Vermilion Editor");
    gtk_window_set_default_size(window_, 1100, 700);
    add_window_actions(G_ACTION_MAP(window_), this);
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

void MainWindow::open_file() {
    opener_.choose();
}

void MainWindow::save() {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        saves_.save(*active);
    }
}

void MainWindow::save_as() {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        saves_.save_as(*active);
    }
}

void MainWindow::close_active() {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        saves_.request_close(*active);
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
    EditorPage* editor = tabs_.active_editor(); // borrowed
    if (editor != nullptr) {
        gtk_widget_activate_action(editor->view(), widget_action, nullptr);
    }
}

void MainWindow::next_tab() {
    tabs_.select_next();
}

void MainWindow::previous_tab() {
    tabs_.select_previous();
}

void MainWindow::set_word_wrap(bool wrap) {
    EditorPage* editor = tabs_.active_editor(); // borrowed
    if (editor != nullptr) {
        editor->set_wrap(wrap);
    }
}

void MainWindow::reopen_as(const std::string& encoding_id) {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        formats_.reopen_as(*active, encoding_id);
    }
}

void MainWindow::save_with_encoding(const std::string& encoding_id) {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        formats_.save_with_encoding(*active, encoding_id);
    }
}

void MainWindow::set_line_ending(const std::string& line_ending_id) {
    std::optional<DocumentId> active = list_.active();
    if (active) {
        formats_.set_line_ending(*active, line_ending_id);
    }
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

// One help window at a time: a second Help > Index brings the open one forward. The window is
// destroyed by GTK (its close button, or with this window); the C++ side goes with it.
void MainWindow::show_help() {
    if (help_) {
        help_->present();
        return;
    }
    GtkApplication* app = gtk_window_get_application(window_); // borrowed
    help_ = std::make_unique<HelpWindow>(app, window_);
    help_destroy_ = gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(help_->window()), "destroy",
                                                    [this](GtkWidget*) { help_.reset(); });
    help_->present();
}

// Unsaved documents are asked about one by one (SaveController); when all are answered, the
// window closes for real.
bool MainWindow::on_close_request() {
    if (quit_confirmed_ || list_.modified_count() == 0) {
        return false;
    }
    saves_.ask_before_quit([this] {
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
    EditorPage* editor = tabs_.active_editor(); // borrowed
    std::optional<DocumentId> active = list_.active();
    const DocumentRecord* record = active ? list_.find(*active) : nullptr;
    bool idle = editor != nullptr && record != nullptr && !editor->busy();
    for (const char* action : kWriteActions) {
        set_enabled(action, idle && !record->read_only);
    }
    set_enabled("reopen-encoding", idle && !record->path.empty());
    // Word wrap belongs to each document, so the menu's check mark follows the active one.
    GAction* wrap = g_action_map_lookup_action(G_ACTION_MAP(window_), "word-wrap"); // borrowed
    if (wrap != nullptr) {
        g_simple_action_set_state(G_SIMPLE_ACTION(wrap),
                                  g_variant_new_boolean(editor != nullptr && editor->wraps()));
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

// Full reference from _new (not floating); show() keeps its own while the dialog is open.
void MainWindow::show_error(const std::string& message, const std::string& detail) {
    auto dialog =
        gtkx::ObjectPtr<GtkAlertDialog>::adopt(gtk_alert_dialog_new("%s", message.c_str()));
    gtk_alert_dialog_set_detail(dialog.get(), detail.c_str());
    gtk_alert_dialog_set_modal(dialog.get(), TRUE);
    gtk_alert_dialog_show(dialog.get(), window_);
}

} // namespace vermilion::ui
