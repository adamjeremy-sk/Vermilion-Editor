// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The strip above a document's text that says its file changed on disk, with two buttons.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

// A horizontal box with a label and two text buttons (the first is the main answer, for example
// "Reload"; the second the other one, for example "Keep mine"). It has the style class
// "vm-change-banner" and starts hidden. It decides nothing: a click calls the matching callback and
// the owner hides the banner, or not. GtkInfoBar is deprecated in GTK 4, hence a plain box.
class ChangeBanner {
public:
    struct Callbacks {
        std::function<void()> primary;   // the first button was clicked
        std::function<void()> secondary; // the second button was clicked
    };

    // The callbacks may hide the banner or show it again; they must not destroy the banner or the
    // page that owns it (the click is still being handled).
    explicit ChangeBanner(Callbacks callbacks);

    ChangeBanner(const ChangeBanner&) = delete;
    ChangeBanner& operator=(const ChangeBanner&) = delete;

    GtkWidget* widget() const { return box_.get(); }

    // Sets the text and the two button labels and makes the banner visible. Showing a banner that
    // is already visible replaces what it says.
    void show(const std::string& text, const std::string& primary_label,
              const std::string& secondary_label);

    void hide();
    bool visible() const;

    // The text that is shown; empty after hide().
    const std::string& text() const { return text_; }

private:
    Callbacks callbacks_;
    gtkx::ObjectPtr<GtkWidget> box_; // the page's box owns it too
    GtkWidget* label_ = nullptr;     // borrowed; owned by box_
    GtkWidget* primary_ = nullptr;   // borrowed; owned by box_
    GtkWidget* secondary_ = nullptr; // borrowed; owned by box_
    std::string text_;
    gtkx::SignalConnection primary_clicked_;
    gtkx::SignalConnection secondary_clicked_;
};

} // namespace vermilion::ui
