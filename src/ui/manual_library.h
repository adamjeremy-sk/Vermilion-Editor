// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// The manual as the program keeps it for the whole session: every topic parsed and the tree of
// topics and headings, prepared on a worker thread after the first window is shown.

#pragma once

#include <glib.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/manual_outline.h"
#include "core/manual_page.h"
#include "core/manual_results.h"
#include "ui/background_task.h"

namespace vermilion::ui {

// All of the manual that does not depend on a window.
struct ManualData {
    // Every topic file that is in the program, parsed; the key is the file name ("index.md").
    std::map<std::string, ManualPage> pages;
    // The tree of the Contents tab: "index.md" first, then the topics it links to (build_outline).
    std::vector<OutlineTopic> outline;
};

// The text of the topic file `name` that is compiled into the program, or nothing when `name` is
// not a topic file name (is_topic_name), the topic is not in the program or it is over 1 MiB.
// Reads resources only: no widget and no state of the program, so a worker thread may call it.
std::optional<std::string> topic_text(const std::string& name);

// The manual made from the text of its topic files (key: file name): every text parsed, the
// outline of "index.md" and the topics it links to. Pure: the same texts give the same result, and
// nothing outside the arguments is read or written.
ManualData make_manual_data(const std::map<std::string, std::string>& texts);

// Reads every topic file in the program and makes the manual of them (make_manual_data). The
// function that the library runs on its worker thread: no widget, no GTK call, no shared state.
ManualData load_manual();

// The pages of the manual for a search, in the order of the tree: file, title (the tree's row) and
// the text as the page view shows it when laid out for `columns` (tables depend on the width).
std::vector<SearchPage> manual_search_pages(const ManualData& data, std::size_t columns);

// The manual, owned by the application for the whole session. prepare() reads it on a worker
// thread at low priority (BackgroundScope); the window that needs it before it is ready shows a
// page that says "Loading..." and asks to be told (on_ready).
class ManualLibrary {
public:
    // A claim on a callback of on_ready(); destroying it, or the library, ends the claim.
    class Wait {
    public:
        Wait() = default;
        Wait(Wait&& other) noexcept;
        Wait& operator=(Wait&& other) noexcept;
        ~Wait();
        Wait(const Wait&) = delete;
        Wait& operator=(const Wait&) = delete;

    private:
        friend class ManualLibrary;
        struct Callbacks {
            std::map<std::uint64_t, std::function<void()>> by_id;
            std::uint64_t next_id = 1;
        };
        Wait(std::weak_ptr<Callbacks> callbacks, std::uint64_t id);
        void release();

        std::weak_ptr<Callbacks> callbacks_;
        std::uint64_t id_ = 0;
    };

    ManualLibrary();
    ~ManualLibrary();

    ManualLibrary(const ManualLibrary&) = delete;
    ManualLibrary& operator=(const ManualLibrary&) = delete;

    // Starts reading the manual now. Nothing when it was started already.
    void prepare();

    // Starts it (prepare()) the next time the main loop has nothing else to do (the lowest
    // priority). Nothing when it was started or asked for already; the request goes when the
    // library does.
    void prepare_when_idle();

    // True from the moment the manual has been handed back on the main loop.
    bool ready() const { return data_.has_value(); }

    // The manual, or null while it is not ready. Once ready the address and the content never
    // change.
    const ManualData* data() const { return data_.has_value() ? &*data_ : nullptr; }

    // `callback` is called once on the main loop when the manual becomes ready, never at once:
    // a caller that finds ready() true has no need of it. It is not called when the returned Wait
    // is destroyed first, or when the library is destroyed first.
    Wait on_ready(std::function<void()> callback);

private:
    static gboolean on_idle(gpointer data);
    void finish(ManualData data);

    std::optional<ManualData> data_;
    std::shared_ptr<Wait::Callbacks> waiting_ = std::make_shared<Wait::Callbacks>();
    bool started_ = false;
    guint idle_source_ = 0; // the pending prepare_when_idle(), or 0
    // Last, so it is destroyed first: a hand-back that has not run yet never runs.
    BackgroundScope scope_;
};

} // namespace vermilion::ui
