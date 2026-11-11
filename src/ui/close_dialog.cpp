// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// GtkAlertDialog with chosen buttons, the last one Cancel; the answer arrives asynchronously.

#include "ui/close_dialog.h"

#include <memory>
#include <utility>

#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

namespace {

using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

// Travels through the asynchronous call; the weak reference tells whether parent still exists.
struct Request {
    Request(GtkWindow* parent, int cancel, std::function<void(int)> callback)
        : cancel_index(cancel), on_answer(std::move(callback)) {
        g_weak_ref_init(&window, parent);
    }
    ~Request() { g_weak_ref_clear(&window); }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    GWeakRef window;
    int cancel_index;
    std::function<void(int)> on_answer;
};

void on_chosen(GObject* source, GAsyncResult* result, gpointer data) {
    std::unique_ptr<Request> request(static_cast<Request*>(data));
    GError* raw_error = nullptr;
    int button = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &raw_error);
    ErrorPtr error(raw_error, &g_error_free);
    // g_weak_ref_get returns a full reference, or nullptr once the window is being destroyed.
    auto window =
        gtkx::ObjectPtr<GObject>::adopt(static_cast<GObject*>(g_weak_ref_get(&request->window)));
    if (!window) {
        return;
    }
    request->on_answer(error || button < 0 ? request->cancel_index : button);
}

} // namespace

void ask(GtkWindow* parent, const std::string& message, const std::string& detail,
         const std::vector<std::string>& buttons, std::function<void(int)> on_answer) {
    // Full reference from _new (not floating); choose() keeps its own while the dialog is open.
    auto dialog =
        gtkx::ObjectPtr<GtkAlertDialog>::adopt(gtk_alert_dialog_new("%s", message.c_str()));
    gtk_alert_dialog_set_detail(dialog.get(), detail.c_str());
    std::vector<const char*> labels;
    for (const std::string& label : buttons) {
        labels.push_back(label.c_str());
    }
    labels.push_back(nullptr);
    gtk_alert_dialog_set_buttons(dialog.get(), labels.data());
    int cancel = static_cast<int>(buttons.size()) - 1;
    gtk_alert_dialog_set_cancel_button(dialog.get(), cancel);
    gtk_alert_dialog_set_default_button(dialog.get(), cancel);
    gtk_alert_dialog_set_modal(dialog.get(), TRUE);
    auto request = std::make_unique<Request>(parent, cancel, std::move(on_answer));
    // on_chosen always runs exactly once and takes the request back.
    gtk_alert_dialog_choose(dialog.get(), parent, nullptr, &on_chosen, request.release());
}

void ask_discard(GtkWindow* parent, const std::string& message, const std::string& detail,
                 const std::string& discard_label, std::function<void()> on_discard) {
    ask(parent, message, detail, {discard_label, "Cancel"},
        [callback = std::move(on_discard)](int button) {
            if (button == 0) {
                callback();
            }
        });
}

} // namespace vermilion::ui
