// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// MainWindow::open_settings: the Settings window, and what Apply does to the program.

#include <memory>
#include <utility>

#include "core/editor_settings.h"
#include "core/external_messages.h"
#include "core/result.h"
#include "core/settings_io.h"
#include "ui/gtkx/signal.h"
#include "ui/main_window.h"
#include "ui/settings_page_appearance.h"
#include "ui/settings_page_editor.h"
#include "ui/settings_page_files.h"
#include "ui/settings_window.h"

namespace vermilion::ui {

void MainWindow::open_settings() {
    if (!settings_window_) {
        SettingsWindow::Host host;
        // Read settings.ini again at every open, so a change made by another program shows.
        host.current = [this] { return reread_settings(settings_); };
        // The file first, then the look.
        host.apply = [this](const EditorSettings& chosen) {
            const Status saved = save_settings(chosen, settings_);
            if (!saved.is_ok()) {
                status_.show_message(settings_save_failed_message(saved.error().message));
            }
            if (appearance_ != nullptr) {
                appearance_->apply(chosen);
            }
        };
        // The focus goes back to the text.
        host.closed = [this] { focus_active_view(); };
        settings_window_ = std::make_unique<SettingsWindow>(window_, std::move(host));
        settings_window_->add_page("Appearance", std::make_unique<AppearancePage>());
        settings_window_->add_page("Editor", std::make_unique<EditorSettingsPage>());
        settings_window_->add_page("Files", std::make_unique<FilesSettingsPage>());
        settings_window_destroy_ =
            gtkx::connect<void(GtkWidget*)>(GTK_WIDGET(settings_window_->window()), "destroy",
                                            [this](GtkWidget*) { settings_window_.reset(); });
    }
    settings_window_->open();
}

} // namespace vermilion::ui
