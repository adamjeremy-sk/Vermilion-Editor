// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// RAII signal connections: a lambda connected to a GObject signal, disconnected on destruction.

#pragma once

#include <glib-object.h>

#include <functional>
#include <memory>
#include <utility>

namespace vermilion::gtkx {

class SignalConnection {
public:
    SignalConnection() = default;
    ~SignalConnection();

    SignalConnection(const SignalConnection&) = delete;
    SignalConnection& operator=(const SignalConnection&) = delete;

    SignalConnection(SignalConnection&& other) noexcept;
    SignalConnection& operator=(SignalConnection&& other) noexcept;

    // True until disconnect() or detach(); it does not say whether the instance is still alive.
    explicit operator bool() const;

    void disconnect();
    void detach();

private:
    struct WeakRefDeleter {
        void operator()(GWeakRef* ref) const {
            g_weak_ref_clear(ref);
            delete ref;
        }
    };
    using WeakRefPtr = std::unique_ptr<GWeakRef, WeakRefDeleter>;

    explicit SignalConnection(gpointer instance, gulong handler_id);

    template <typename Sig, typename F>
    friend SignalConnection connect(gpointer instance, const char* signal_name, F&& callback);

    WeakRefPtr weak_;
    gulong handler_id_ = 0;
    bool active_ = false;
};

namespace detail {

template <typename Sig>
struct Holder;

template <typename R, typename... Args>
struct Holder<R(Args...)> {
    std::function<R(Args...)> fn;
};

template <typename Sig>
struct Trampoline;

template <typename R, typename... Args>
struct Trampoline<R(Args...)> {
    static R call(Args... args, gpointer user_data) {
        auto* holder = static_cast<Holder<R(Args...)>*>(user_data);
        return holder->fn(args...);
    }
};

template <typename Sig>
void holder_destroy_notify(gpointer data, GClosure* closure) {
    (void)closure;
    delete static_cast<Holder<Sig>*>(data);
}

} // namespace detail

template <typename Sig, typename F>
SignalConnection connect(gpointer instance, const char* signal_name, F&& callback) {
    if (instance == nullptr) {
        return SignalConnection();
    }
    std::unique_ptr<detail::Holder<Sig>> holder(new detail::Holder<Sig>());
    holder->fn = std::forward<F>(callback);
    gulong id =
        g_signal_connect_data(instance, signal_name, G_CALLBACK(&detail::Trampoline<Sig>::call),
                              holder.get(), &detail::holder_destroy_notify<Sig>, G_CONNECT_DEFAULT);
    if (id == 0) {
        return SignalConnection();
    }
    holder.release();
    return SignalConnection(instance, id);
}

} // namespace vermilion::gtkx
