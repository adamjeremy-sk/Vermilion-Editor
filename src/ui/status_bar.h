// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Status bar of the main window: message, Language, Tab Width, Encoding, Line Ending, Ln/Col, mode.

#pragma once

#include <gtk/gtk.h>

#include <array>
#include <string>

#include "ui/gtkx/object_ptr.h"

namespace vermilion::ui {

// What the status bar shows for the active document.
struct CursorStatus {
    int line = 1;   // 1-based
    int column = 1; // 1-based, a tab counts as the columns it covers
    bool overwrite = false;
    unsigned tab_width = 4;
    std::string language = "Plain Text";
    std::string encoding = "UTF-8";
    std::string line_ending = "LF"; // "Mixed" while a file keeps more than one kind
};

class StatusBar {
public:
    StatusBar();

    StatusBar(const StatusBar&) = delete;
    StatusBar& operator=(const StatusBar&) = delete;

    GtkWidget* widget() const { return box_.get(); }

    void show_document(const CursorStatus& status);

    // Hides the document panels; only the message panel stays.
    void show_no_document();

    // Replaces the text of the message panel at the left.
    void show_message(const std::string& message);

private:
    std::array<GtkWidget*, 6> panels() const;

    gtkx::ObjectPtr<GtkWidget> box_;
    GtkWidget* message_ = nullptr; // the labels are borrowed; box_ owns them
    GtkWidget* language_ = nullptr;
    GtkWidget* tab_width_ = nullptr;
    GtkWidget* encoding_ = nullptr;
    GtkWidget* line_ending_ = nullptr;
    GtkWidget* position_ = nullptr;
    GtkWidget* mode_ = nullptr;
};

} // namespace vermilion::ui
