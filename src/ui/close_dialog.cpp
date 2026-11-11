// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// GtkAlertDialog with a discard button and Cancel; the answer arrives asynchronously.

#include "ui/close_dialog.h"

#include <memory>
#include <utility>

#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

namespace {

constexpr int kDiscardButton = 0;
constexpr int kCancelButton = 1;

using ErrorPtr = std::unique_ptr<GError, decltype(&g_error_free)>;

// Travels through the asynchronous call; the weak reference tells whether parent still exists.
struct Request {
    Request(GtkWindow* parent, std::function<void()> callback) : on_discard(std::move(callback)) {
        g_weak_ref_init(&window, parent);
    }
    ~Request() { g_weak_ref_clear(&window); }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    GWeakRef window;
    std::function<void()> on_discard;
};

void on_answer(GObject* source, GAsyncResult* result, gpointer data) {
    std::unique_ptr<Request> request(static_cast<Request*>(data));
    GError* raw_error = nullptr;
    int button = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &raw_error);
    ErrorPtr error(raw_error, &g_error_free);
    // g_weak_ref_get returns a full reference, or nullptr once the window is being destroyed.
    auto window =
        gtkx::ObjectPtr<GObject>::adopt(static_cast<GObject*>(g_weak_ref_get(&request->window)));
    if (error || button != kDiscardButton || !window) {
        return;
    }
    request->on_discard();
}

} // namespace

void ask_discard(GtkWindow* parent, const std::string& message, const std::string& detail,
                 const std::string& discard_label, std::function<void()> on_discard) {
    // Full reference from _new (not floating); choose() keeps its own while the dialog is open.
    auto dialog =
        gtkx::ObjectPtr<GtkAlertDialog>::adopt(gtk_alert_dialog_new("%s", message.c_str()));
    gtk_alert_dialog_set_detail(dialog.get(), detail.c_str());
    const char* const buttons[] = {discard_label.c_str(), "Cancel", nullptr};
    gtk_alert_dialog_set_buttons(dialog.get(), buttons);
    gtk_alert_dialog_set_cancel_button(dialog.get(), kCancelButton);
    gtk_alert_dialog_set_default_button(dialog.get(), kCancelButton);
    gtk_alert_dialog_set_modal(dialog.get(), TRUE);
    auto request = std::make_unique<Request>(parent, std::move(on_discard));
    // on_answer always runs exactly once and takes the request back.
    gtk_alert_dialog_choose(dialog.get(), parent, nullptr, &on_answer, request.release());
}

} // namespace vermilion::ui
