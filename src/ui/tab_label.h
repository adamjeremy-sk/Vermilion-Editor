// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// One document tab's label: the title at a fixed width, the unsaved marker and the close button.

#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

namespace vermilion::ui {

class TabLabel {
public:
    struct Callbacks {
        std::function<void()> close;     // close button clicked, or middle click on the tab
        std::function<void(int)> scroll; // mouse wheel over the tab: +1 next tab, -1 previous
    };

    explicit TabLabel(Callbacks callbacks);

    TabLabel(const TabLabel&) = delete;
    TabLabel& operator=(const TabLabel&) = delete;

    GtkWidget* widget() const { return box_.get(); }

    // Shows "*title" while the document has unsaved changes; the tooltip is the title until
    // documents have paths (M2.2).
    void set(const std::string& title, bool modified);

    // True when widget is the close button or inside it.
    bool contains_close_button(GtkWidget* widget) const;

private:
    Callbacks callbacks_;
    gtkx::ObjectPtr<GtkWidget> box_;
    GtkWidget* title_ = nullptr; // borrowed; box_ owns it
    GtkWidget* close_ = nullptr; // borrowed; box_ owns it
    gtkx::SignalConnection clicked_;
    gtkx::SignalConnection middle_released_;
    gtkx::SignalConnection wheel_;
};

} // namespace vermilion::ui
