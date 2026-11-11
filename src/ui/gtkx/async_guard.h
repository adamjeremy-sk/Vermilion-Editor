// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Lets an asynchronous callback find out whether the object that started the call still exists.

#pragma once

#include <glib.h>

#include <memory>

namespace vermilion::gtkx {

// Owned by the object that starts asynchronous calls (a member, declared last so it goes first).
// Each call gets a ticket as its user_data; the callback takes the ticket back and gets the owner,
// or nullptr once the owner is gone. Every ticket must be taken back exactly once: with take() in
// a ready callback, or with free_ticket() as the GDestroyNotify of a callback that runs many
// times and reads its ticket with peek().
template <typename T>
class AsyncGuard {
public:
    explicit AsyncGuard(T* owner) : state_(std::make_shared<State>(State{owner})) {}
    ~AsyncGuard() { state_->owner = nullptr; }

    AsyncGuard(const AsyncGuard&) = delete;
    AsyncGuard& operator=(const AsyncGuard&) = delete;

    gpointer ticket() const { return std::make_unique<std::shared_ptr<State>>(state_).release(); }

    static T* take(gpointer ticket) {
        std::unique_ptr<std::shared_ptr<State>> held(static_cast<std::shared_ptr<State>*>(ticket));
        return (*held)->owner;
    }

    static T* peek(gpointer ticket) {
        return (*static_cast<std::shared_ptr<State>*>(ticket))->owner;
    }

    static void free_ticket(gpointer ticket) { take(ticket); }

private:
    struct State {
        T* owner;
    };

    std::shared_ptr<State> state_;
};

} // namespace vermilion::gtkx
