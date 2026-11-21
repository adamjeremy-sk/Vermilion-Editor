// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Brings the program's look up to date with the settings, 100 ms after the last change.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "core/colour_mode.h"
#include "core/editor_look.h"
#include "core/editor_settings.h"
#include "ui/debounce_timer.h"

namespace vermilion::ui {

// Holds the settings that are wanted and decides what has to change in the program. It knows no
// widget: the Host does the work. The wait after the last change is the debounce of the Settings
// window (Adam, 2026-11-19): Apply pressed again within 100 ms replaces the values still waiting,
// so the last values win. Main loop only.
class AppearanceController {
public:
    // What the controller changes. Each function gets the new value of one part of the look.
    class Host {
    public:
        virtual ~Host() = default;

        // The palette of the stylesheet, and GTK's own widgets with it: dark or light.
        virtual void apply_palette(bool dark) = 0;

        // The editor pages, the ones that are open and the ones made later.
        virtual void apply_look(const EditorLook& look) = 0;

        // The longest line, in characters, above which opening a file asks first.
        virtual void apply_long_line_limit(std::uint64_t limit) = 0;
    };

    // The wait after the last apply() or set_hints().
    static constexpr unsigned kDelayMs = 100;

    // `host` is borrowed and must outlive the controller. `timer` is not null; the controller
    // stops it when it is destroyed, so a waiting change is dropped and calls nothing.
    AppearanceController(Host& host, std::unique_ptr<DebounceTimer> timer);
    ~AppearanceController();

    AppearanceController(const AppearanceController&) = delete;
    AppearanceController& operator=(const AppearanceController&) = delete;

    // The first call, at start-up: normalises the values (normalise_settings), drops a change that
    // is waiting and applies at once, so the first window never shows the wrong look. Later calls
    // do the same. Whatever has not changed since the last application is not applied again; the
    // first application gives the Host all three parts.
    void start(const EditorSettings& values);

    // Normalises the values and applies them 100 ms later, unless apply() or set_hints() comes
    // first. Before the first start() it only keeps the values.
    void apply(const EditorSettings& values);

    // What the desktop says (core/colour_mode.h); it decides what the colour mode System means.
    // Keeps the hints and, like apply(), brings the look up to date after the wait. Before the
    // first start() it only keeps them. Without hints, System means light.
    void set_hints(const SystemHints& hints);

    // The values that are wanted: the last given to start() or apply(), normalised, whether or not
    // they have been applied yet.
    const EditorSettings& settings() const { return settings_; }

    // True while a change waits for its 100 ms.
    bool pending() const { return pending_; }

private:
    // Starts the wait, if start() has been called.
    void schedule();

    // Works out the palette, the look and the limit for the values and the hints, and gives the
    // Host the parts that differ from what it was given before, in this order: palette, look,
    // limit.
    void flush();

    Host& host_;
    std::unique_ptr<DebounceTimer> timer_;
    EditorSettings settings_;
    SystemHints hints_;
    bool started_ = false;
    bool pending_ = false;
    std::optional<bool> applied_dark_;
    std::optional<EditorLook> applied_look_;
    std::optional<std::uint64_t> applied_limit_;
};

} // namespace vermilion::ui
