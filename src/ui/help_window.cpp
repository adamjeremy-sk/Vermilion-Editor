// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the help window: the buttons and the page view that shows one page of the manual.

#include "ui/help_window.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/manual_page.h"
#include "core/manual_spans.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {
namespace {

constexpr const char* kManualPrefix = "/io/github/adamjeremy_sk/Vermilion/manual/";
constexpr const char* kIndexTopic = "index.md";
constexpr gsize kMaxTopicBytes = 1024 * 1024;

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;

std::optional<std::string> topic_text(const std::string& name) {
    if (!is_topic_name(name)) {
        return std::nullopt;
    }
    std::string path = kManualPrefix + name;
    BytesPtr bytes(g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr),
                   &g_bytes_unref); // full reference; nullptr when the topic is not in the program
    if (bytes == nullptr) {
        return std::nullopt;
    }
    gsize size = 0;
    const void* data = g_bytes_get_data(bytes.get(), &size);
    if (size > kMaxTopicBytes) {
        return std::nullopt;
    }
    return std::string(static_cast<const char*>(data), size);
}

void on_back(GSimpleAction*, GVariant*, gpointer data) {
    static_cast<HelpWindow*>(data)->back();
}

void on_to_index(GSimpleAction*, GVariant*, gpointer data) {
    static_cast<HelpWindow*>(data)->show_index();
}

const GActionEntry kActions[] = {
    {"back", on_back, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"to-index", on_to_index, nullptr, nullptr, nullptr, {0, 0, 0}},
};

// Floating until the bar takes it over.
GtkWidget* action_button(const char* label, const char* action, const char* tooltip) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action);
    gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

// The bar with the Back and Index buttons; floating until the root box takes it over.
GtkWidget* build_bar() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(bar, "vm-help-bar");
    gtk_box_append(GTK_BOX(bar), action_button("Back", "win.back", "Back (Alt+Left)"));
    gtk_box_append(GTK_BOX(bar), action_button("Index", "win.to-index", "Index"));
    return bar;
}

// The natural height of `widget`, in pixels.
int natural_height(GtkWidget* widget) {
    int natural = 0;
    gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL, -1, nullptr, &natural, nullptr, nullptr);
    return natural;
}

} // namespace

HelpWindow::HelpWindow(GtkApplication* app, GtkWindow* parent)
    : window_(GTK_WINDOW(gtk_application_window_new(app))) { // GTK owns the window; it is borrowed
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window_), FALSE);
    gtk_window_set_title(window_, "Vermilion Editor Help");
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-help");
    g_action_map_add_action_entries(G_ACTION_MAP(window_), kActions, G_N_ELEMENTS(kActions), this);
    const char* const back_keys[] = {"<Alt>Left", nullptr};
    gtk_application_set_accels_for_action(app, "win.back", back_keys);
    page_view_ =
        std::make_unique<HelpPageView>([this](const std::string& target) { open_topic(target); });
    GtkWidget* page = page_view_->widget(); // borrowed; the page view owns it until the box has it
    gtk_widget_set_vexpand(page, TRUE);
    // Floating; the window takes it over.
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* bar = build_bar(); // floating; the root box takes it over
    gtk_box_append(GTK_BOX(root), bar);
    gtk_box_append(GTK_BOX(root), page);
    gtk_window_set_child(window_, root);
    // The window opens kHelpOpenColumns columns wide and kHelpPageLines lines high below the bar.
    gtk_window_set_default_size(window_, page_view_->width_for_columns(kHelpOpenColumns),
                                natural_height(bar) + page_view_->height_for_lines(kHelpPageLines));
    GtkSettings* settings = gtk_settings_get_default(); // borrowed; null without a display
    if (settings != nullptr) {
        palette_signal_ = gtkx::connect<void(GObject*, GParamSpec*)>(
            settings, "notify::gtk-application-prefer-dark-theme",
            [this](GObject*, GParamSpec*) { page_view_->refresh_colours(); });
    }
    open_topic(kIndexTopic);
}

void HelpWindow::present() {
    gtk_window_present(window_);
}

void HelpWindow::back() {
    std::optional<std::string> page = history_.back();
    if (page.has_value()) {
        show(*page);
        update_actions();
    }
}

void HelpWindow::show_index() {
    open_topic(kIndexTopic);
}

void HelpWindow::open_topic(const std::string& name) {
    if (is_topic_name(name) && history_.open(name)) {
        show(name);
        update_actions();
    }
}

void HelpWindow::show(const std::string& name) {
    std::optional<std::string> text = topic_text(name);
    ManualPage page;
    if (text.has_value()) {
        page = parse_manual(*text);
    } else {
        ManualBlock block;
        block.spans.resize(1);
        block.spans[0].kind = SpanKind::Text;
        block.spans[0].text = "This topic is not available: " + name;
        page.blocks.push_back(std::move(block));
    }
    page_view_->show_source(page);
}

void HelpWindow::update_actions() {
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(window_), "back"); // borrowed
    if (action != nullptr) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), history_.can_go_back() ? TRUE : FALSE);
    }
}

} // namespace vermilion::ui
