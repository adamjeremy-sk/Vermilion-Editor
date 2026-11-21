// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The GActionEntry table of the main window, its handlers, and the shortcut table.

#include "ui/window_actions.h"

#include "ui/main_window.h"

namespace vermilion::ui {

namespace {

MainWindow* self(gpointer data) {
    return static_cast<MainWindow*>(data);
}

// The parameter of an action with type "s" (borrowed).
const char* text_of(GVariant* parameter) {
    return g_variant_get_string(parameter, nullptr);
}

void on_new_file(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->new_file();
}
void on_open(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->open_file();
}
void on_save(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->save();
}
void on_save_as(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->save_as();
}
void on_reload_from_disk(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->reload_from_disk();
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
void on_find(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->find();
}
void on_find_next(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->find_next();
}
void on_find_previous(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->find_previous();
}
void on_replace(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->replace();
}
void on_go_to_line(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->go_to_line();
}
void on_next_tab(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->next_tab();
}
void on_previous_tab(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->previous_tab();
}
// The menu item toggles the boolean state (GSimpleAction's default for an action without an
// activate handler); the new state is applied here and then stored in the action.
void on_word_wrap(GSimpleAction* action, GVariant* value, gpointer data) {
    self(data)->set_word_wrap(g_variant_get_boolean(value) != FALSE);
    g_simple_action_set_state(action, value);
}
void on_reopen_encoding(GSimpleAction*, GVariant* parameter, gpointer data) {
    self(data)->reopen_as(text_of(parameter));
}
void on_save_encoding(GSimpleAction*, GVariant* parameter, gpointer data) {
    self(data)->save_with_encoding(text_of(parameter));
}
void on_line_ending(GSimpleAction*, GVariant* parameter, gpointer data) {
    self(data)->set_line_ending(text_of(parameter));
}
void on_about(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->about();
}
void on_help_index(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->show_help();
}
void on_settings(GSimpleAction*, GVariant*, gpointer data) {
    self(data)->open_settings();
}

const GActionEntry kActions[] = {
    {"new-file", on_new_file, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"open", on_open, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"save", on_save, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"save-as", on_save_as, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"reload-from-disk", on_reload_from_disk, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"close-tab", on_close_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"quit", on_quit, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"undo", on_undo, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"redo", on_redo, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"cut", on_cut, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"copy", on_copy, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"paste", on_paste, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"select-all", on_select_all, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find", on_find, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find-next", on_find_next, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"find-previous", on_find_previous, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"replace", on_replace, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"go-to-line", on_go_to_line, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"next-tab", on_next_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"previous-tab", on_previous_tab, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"word-wrap", nullptr, nullptr, "false", on_word_wrap, {0, 0, 0}},
    {"reopen-encoding", on_reopen_encoding, "s", nullptr, nullptr, {0, 0, 0}},
    {"save-encoding", on_save_encoding, "s", nullptr, nullptr, {0, 0, 0}},
    {"line-ending", on_line_ending, "s", nullptr, nullptr, {0, 0, 0}},
    {"settings", on_settings, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"help-index", on_help_index, nullptr, nullptr, nullptr, {0, 0, 0}},
    {"about", on_about, nullptr, nullptr, nullptr, {0, 0, 0}},
};

} // namespace

void add_window_actions(GActionMap* map, MainWindow* window) {
    g_action_map_add_action_entries(map, kActions, G_N_ELEMENTS(kActions), window);
}

} // namespace vermilion::ui
