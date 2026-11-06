// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include "ui/gtkx/signal.h"

namespace vermilion::gtkx {

SignalConnection::SignalConnection(gpointer instance, gulong handler_id)
    : handler_id_(handler_id), active_(true) {
    GWeakRef* ref = new GWeakRef();
    g_weak_ref_init(ref, instance);
    weak_.reset(ref);
}

SignalConnection::SignalConnection(SignalConnection&& other) noexcept
    : weak_(std::move(other.weak_)),
      handler_id_(other.handler_id_),
      active_(other.active_) {
    other.weak_ = nullptr;
    other.handler_id_ = 0;
    other.active_ = false;
}

SignalConnection& SignalConnection::operator=(SignalConnection&& other) noexcept {
    if (this != &other) {
        disconnect();
        weak_ = std::move(other.weak_);
        handler_id_ = other.handler_id_;
        active_ = other.active_;
        other.weak_ = nullptr;
        other.handler_id_ = 0;
        other.active_ = false;
    }
    return *this;
}

SignalConnection::~SignalConnection() {
    disconnect();
}

SignalConnection::operator bool() const {
    return active_;
}

void SignalConnection::disconnect() {
    if (!active_) {
        return;
    }
    if (weak_ != nullptr) {
        GObject* instance = static_cast<GObject*>(g_weak_ref_get(weak_.get()));
        if (instance != nullptr) {
            g_signal_handler_disconnect(instance, handler_id_);
            g_object_unref(instance);
        }
    }
    active_ = false;
}

void SignalConnection::detach() {
    active_ = false;
}

}  // namespace vermilion::gtkx
