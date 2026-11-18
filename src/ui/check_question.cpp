// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the question window with a check box and answers it once.

#include "ui/check_question.h"

#include <memory>
#include <utility>

#include "ui/gtkx/signal.h"

namespace vermilion::ui {
namespace {

constexpr int kWindowWidth = 420;
constexpr int kMargin = 18;
constexpr int kBoxSpacing = 12;
constexpr int kButtonSpacing = 6;
constexpr int kWrapChars = 48;

// The question's state, shared by every handler of the window. It lives in a shared_ptr that the
// handlers capture by value; it is freed with the window, when GTK frees the handlers.
struct Question {
    std::function<void(bool, bool)> on_answer;
    GtkWindow* window = nullptr; // borrowed
    GtkWidget* check = nullptr;  // borrowed; nullptr when no check box was made
    bool answered = false;
};

// The two buttons and the box that holds them (floating until the caller appends it).
struct Buttons {
    GtkWidget* box = nullptr;
    GtkWidget* go = nullptr;     // borrowed
    GtkWidget* cancel = nullptr; // borrowed
};

// Answers the question once and closes the window. Every way out (a button, Escape, the title bar)
// goes through here, so on_answer runs exactly once.
void answer(Question& question, bool go) {
    if (question.answered) {
        return;
    }
    question.answered = true;
    const bool ticked = go && question.check != nullptr &&
                        gtk_check_button_get_active(GTK_CHECK_BUTTON(question.check)) != FALSE;
    std::function<void(bool, bool)> callback = std::move(question.on_answer);
    GtkWindow* window = question.window;
    // After this the window and its handlers are gone; touch only the locals copied above.
    gtk_window_destroy(window);
    if (callback) {
        callback(go, ticked);
    }
}

// Builds the message or the detail line (the message carries the css class, the detail does not).
GtkWidget* make_label(const std::string& text, bool message_line) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), kWrapChars);
    if (message_line) {
        gtk_widget_add_css_class(label, "vm-ask-message");
    }
    return label;
}

// The empty modal window over parent, with its title, size and css classes.
GtkWindow* make_window(GtkWindow* parent) {
    GtkWindow* window = GTK_WINDOW(gtk_window_new()); // GTK owns it; borrowed here
    gtk_window_set_title(window, "Vermilion Editor");
    gtk_window_set_transient_for(window, parent);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_destroy_with_parent(window, TRUE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_default_size(window, kWindowWidth, -1);
    gtk_widget_add_css_class(GTK_WIDGET(window), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window), "vm-ask");
    return window;
}

// The vertical box that holds the whole question, with its margins (floating; the window takes it
// over).
GtkWidget* make_content_box() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, kBoxSpacing);
    gtk_widget_set_margin_top(box, kMargin);
    gtk_widget_set_margin_bottom(box, kMargin);
    gtk_widget_set_margin_start(box, kMargin);
    gtk_widget_set_margin_end(box, kMargin);
    return box;
}

// The go button, then Cancel (the last button is Cancel), right-aligned in one box.
Buttons make_buttons(const std::string& go_label) {
    Buttons buttons;
    buttons.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kButtonSpacing);
    gtk_widget_set_halign(buttons.box, GTK_ALIGN_END);
    buttons.go = gtk_button_new_with_label(go_label.c_str());
    buttons.cancel = gtk_button_new_with_label("Cancel");
    gtk_box_append(GTK_BOX(buttons.box), buttons.go);
    gtk_box_append(GTK_BOX(buttons.box), buttons.cancel);
    return buttons;
}

// The handlers stay connected until the window is destroyed; each connection is detached at once,
// because the connection object has nothing left to do.
void connect_answers(std::shared_ptr<Question> question, const Buttons& buttons) {
    gtkx::connect<void(GtkButton*)>(buttons.go, "clicked", [question](GtkButton*) {
        answer(*question, true);
    }).detach();
    gtkx::connect<void(GtkButton*)>(buttons.cancel, "clicked", [question](GtkButton*) {
        answer(*question, false);
    }).detach();
    gtkx::connect<gboolean(GtkWindow*)>(question->window, "close-request",
                                        [question](GtkWindow*) -> gboolean {
                                            answer(*question, false);
                                            return TRUE;
                                        })
        .detach();
}

// Escape answers Cancel. The controller runs in the capture phase, so no widget below the window
// (the check box, a button) can take the key first.
void connect_escape(std::shared_ptr<Question> question) {
    GtkEventController* keys = gtk_event_controller_key_new(); // floating; the window takes it over
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    gtkx::connect<gboolean(GtkEventControllerKey*, guint, guint, GdkModifierType)>(
        keys, "key-pressed",
        [question](GtkEventControllerKey*, guint keyval, guint, GdkModifierType) -> gboolean {
            if (keyval == GDK_KEY_Escape) {
                answer(*question, false);
                return TRUE;
            }
            return FALSE;
        })
        .detach();
    gtk_widget_add_controller(GTK_WIDGET(question->window), keys);
}

} // namespace

void ask_with_check(GtkWindow* parent, const std::string& message, const std::string& detail,
                    const std::string& go_label, const std::string& check_label,
                    std::function<void(bool go, bool ticked)> on_answer) {
    GtkWindow* window = make_window(parent);
    GtkWidget* box = make_content_box();
    gtk_box_append(GTK_BOX(box), make_label(message, true));
    if (!detail.empty()) {
        gtk_box_append(GTK_BOX(box), make_label(detail, false));
    }

    auto question = std::make_shared<Question>();
    question->on_answer = std::move(on_answer);
    question->window = window;
    if (!check_label.empty()) {
        question->check = gtk_check_button_new_with_label(check_label.c_str()); // not ticked
        gtk_box_append(GTK_BOX(box), question->check);
    }

    const Buttons buttons = make_buttons(go_label);
    gtk_box_append(GTK_BOX(box), buttons.box);
    gtk_window_set_child(window, box);

    // Cancel is the default button and has the focus at first, so Enter never goes ahead.
    gtk_window_set_default_widget(window, buttons.cancel);
    gtk_widget_grab_focus(buttons.cancel);

    connect_answers(question, buttons);
    connect_escape(question);
    gtk_window_present(window);
}

} // namespace vermilion::ui
