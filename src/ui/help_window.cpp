// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Builds the help window: the manual's pages as labels and grids in a scrolling area.

#include "ui/help_window.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/manual_page.h"
#include "core/manual_spans.h"
#include "ui/gtkx/signal.h"
#include "ui/gtkx/string.h"

namespace vermilion::ui {
namespace {

constexpr const char* kManualPrefix = "/io/github/adamjeremy_sk/Vermilion/manual/";
constexpr const char* kIndexTopic = "index.md";
constexpr gsize kMaxTopicBytes = 1024 * 1024;
constexpr int kTextChars = 90;
constexpr int kCellChars = 50;

using BytesPtr = std::unique_ptr<GBytes, decltype(&g_bytes_unref)>;

// Bad bytes are replaced, then the text is escaped for markup.
std::string escape(const std::string& text) {
    std::string valid = gtkx::take_string(g_utf8_make_valid(text.c_str(), -1));
    return gtkx::take_string(g_markup_escape_text(valid.c_str(), -1));
}

std::string markup_of(const std::vector<ManualSpan>& spans, bool& has_link) {
    std::string out;
    for (const auto& span : spans) {
        std::string text = escape(span.text);
        switch (span.kind) {
        case SpanKind::Text:
            out += text;
            break;
        case SpanKind::Bold:
            out += "<b>";
            out += text;
            out += "</b>";
            break;
        case SpanKind::Code:
            out += "<tt>";
            out += text;
            out += "</tt>";
            break;
        case SpanKind::Link:
            out += "<a href=\"";
            out += escape(span.target);
            out += "\">";
            out += text;
            out += "</a>";
            has_link = true;
            break;
        }
    }
    return out;
}

// Floating until the container it is appended to takes it over.
GtkWidget* span_label(const std::vector<ManualSpan>& spans, int chars,
                      std::vector<GtkWidget*>& links) {
    bool has_link = false;
    std::string markup = markup_of(spans, has_link);
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(label), chars);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    if (has_link) {
        links.push_back(label); // borrowed; only used by HelpWindow::show
    }
    return label;
}

GtkWidget* heading_widget(const ManualBlock& block, std::vector<GtkWidget*>& links) {
    GtkWidget* label = span_label(block.spans, kTextChars, links);
    const char* css_class = "vm-help-h3";
    if (block.level == 1) {
        css_class = "vm-help-h1";
    } else if (block.level == 2) {
        css_class = "vm-help-h2";
    }
    gtk_widget_add_css_class(label, css_class);
    return label;
}

GtkWidget* item_widget(const ManualBlock& block, std::vector<GtkWidget*>& links) {
    // Floating; the page box takes it over.
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(box, "vm-help-item");
    std::vector<ManualSpan> bullet;
    bullet.push_back(ManualSpan{SpanKind::Text, "\u2022", ""});
    GtkWidget* bullet_label = span_label(bullet, 1, links);
    gtk_widget_add_css_class(bullet_label, "vm-help-bullet");
    gtk_widget_set_valign(bullet_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), bullet_label);
    gtk_box_append(GTK_BOX(box), span_label(block.spans, kTextChars, links));
    return box;
}

GtkWidget* table_widget(const ManualBlock& block, std::vector<GtkWidget*>& links) {
    GtkWidget* grid = gtk_grid_new(); // floating; the page box takes it over
    gtk_widget_add_css_class(grid, "vm-help-table");
    gtk_widget_set_halign(grid, GTK_ALIGN_START);
    for (std::size_t r = 0; r < block.rows.size(); ++r) {
        for (std::size_t c = 0; c < block.columns; ++c) {
            ManualCell empty_cell;
            const ManualCell& cell = c < block.rows[r].size() ? block.rows[r][c] : empty_cell;
            GtkWidget* label = span_label(cell, kCellChars, links);
            gtk_widget_add_css_class(label, r == 0 ? "vm-help-th" : "vm-help-td");
            gtk_grid_attach(GTK_GRID(grid), label, static_cast<int>(c), static_cast<int>(r), 1, 1);
        }
    }
    return grid;
}

GtkWidget* block_widget(const ManualBlock& block, std::vector<GtkWidget*>& links) {
    switch (block.kind) {
    case ManualBlockKind::Heading:
        return heading_widget(block, links);
    case ManualBlockKind::ListItem:
        return item_widget(block, links);
    case ManualBlockKind::Table:
        return table_widget(block, links);
    case ManualBlockKind::Paragraph:
        break;
    }
    return span_label(block.spans, kTextChars, links);
}

// Floating until the scrolled window takes it over.
GtkWidget* page_widget(const ManualPage& page, std::vector<GtkWidget*>& links) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(box, "vm-help-page");
    for (const auto& block : page.blocks) {
        gtk_box_append(GTK_BOX(box), block_widget(block, links));
    }
    return box;
}

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

} // namespace

HelpWindow::HelpWindow(GtkApplication* app, GtkWindow* parent)
    : window_(GTK_WINDOW(gtk_application_window_new(app))) { // GTK owns the window; it is borrowed
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window_), FALSE);
    gtk_window_set_title(window_, "Vermilion Editor Help");
    gtk_window_set_default_size(window_, 780, 640);
    gtk_window_set_transient_for(window_, parent);
    gtk_window_set_destroy_with_parent(window_, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-main");
    gtk_widget_add_css_class(GTK_WIDGET(window_), "vm-help");
    g_action_map_add_action_entries(G_ACTION_MAP(window_), kActions, G_N_ELEMENTS(kActions), this);
    const char* const back_keys[] = {"<Alt>Left", nullptr};
    gtk_application_set_accels_for_action(app, "win.back", back_keys);
    // Floating; the window takes the root box over.
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // Floating; the root box takes the bar over.
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(bar, "vm-help-bar");
    gtk_box_append(GTK_BOX(bar), action_button("Back", "win.back", "Back (Alt+Left)"));
    gtk_box_append(GTK_BOX(bar), action_button("Index", "win.to-index", "Index"));
    gtk_box_append(GTK_BOX(root), bar);
    scroller_ = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new()); // floating; the root box owns it
    gtk_widget_add_css_class(GTK_WIDGET(scroller_), "vm-help-well");
    gtk_scrolled_window_set_policy(scroller_, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(GTK_WIDGET(scroller_), TRUE);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(scroller_));
    gtk_window_set_child(window_, root);
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
    if (history_.open(name)) {
        show(name);
        update_actions();
    }
}

void HelpWindow::show(const std::string& name) {
    // This may run inside a link click; the old connections are released here.
    link_signals_.clear();
    std::vector<GtkWidget*> links;
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
    GtkWidget* content = page_widget(page, links);
    for (GtkWidget* label : links) {
        link_signals_.push_back(gtkx::connect<gboolean(GtkLabel*, const char*)>(
            label, "activate-link", [this](GtkLabel*, const char* uri) -> gboolean {
                return follow_link(uri) ? TRUE : FALSE;
            }));
    }
    // Takes the floating box and releases the old page.
    gtk_scrolled_window_set_child(scroller_, content);
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(scroller_), 0.0);
}

void HelpWindow::update_actions() {
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(window_), "back"); // borrowed
    if (action != nullptr) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), history_.can_go_back() ? TRUE : FALSE);
    }
}

bool HelpWindow::follow_link(const char* uri) {
    if (uri != nullptr && is_topic_name(uri)) {
        open_topic(uri);
    }
    return true;
}

} // namespace vermilion::ui
