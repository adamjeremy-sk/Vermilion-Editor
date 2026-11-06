// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#pragma once

#include <glib-object.h>

#include <utility>

namespace vermilion::gtkx {

template <typename T>
class ObjectPtr {
public:
    ObjectPtr() = default;

    static ObjectPtr adopt(T* object) {
        return ObjectPtr(object);
    }

    static ObjectPtr adopt_floating(T* object) {
        if (object == nullptr) {
            return ObjectPtr();
        }
        return ObjectPtr(static_cast<T*>(g_object_ref_sink(object)));
    }

    static ObjectPtr ref(T* object) {
        if (object == nullptr) {
            return ObjectPtr();
        }
        return ObjectPtr(static_cast<T*>(g_object_ref(object)));
    }

    ObjectPtr(const ObjectPtr& other) : object_(other.object_) {
        if (object_ != nullptr) {
            g_object_ref(object_);
        }
    }

    ObjectPtr(ObjectPtr&& other) noexcept : object_(other.object_) {
        other.object_ = nullptr;
    }

    ObjectPtr& operator=(const ObjectPtr& other) {
        if (this != &other) {
            ObjectPtr copy(other);
            std::swap(object_, copy.object_);
        }
        return *this;
    }

    ObjectPtr& operator=(ObjectPtr&& other) noexcept {
        if (this != &other) {
            if (object_ != nullptr) {
                g_object_unref(object_);
            }
            object_ = other.object_;
            other.object_ = nullptr;
        }
        return *this;
    }

    ~ObjectPtr() {
        if (object_ != nullptr) {
            g_object_unref(object_);
        }
    }

    T* get() const {
        return object_;
    }

    T* operator->() const {
        return object_;
    }

    explicit operator bool() const {
        return object_ != nullptr;
    }

    T* release() {
        T* object = object_;
        object_ = nullptr;
        return object;
    }

    void reset() {
        if (object_ != nullptr) {
            g_object_unref(object_);
            object_ = nullptr;
        }
    }

private:
    explicit ObjectPtr(T* object) : object_(object) {}

    T* object_ = nullptr;
};

}  // namespace vermilion::gtkx
