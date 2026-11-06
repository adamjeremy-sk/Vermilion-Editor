// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

#include <gtest/gtest.h>

#include <glib-object.h>

#include "ui/gtkx/object_ptr.h"

using vermilion::gtkx::ObjectPtr;

template <typename T>
static void add_weak(T* object, void** weak) {
    g_object_add_weak_pointer(G_OBJECT(object), weak);
}

TEST(GtkxObjectPtr, AdoptReleasesOnScopeExit) {
    void* weak = nullptr;
    {
        GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
        weak = object;
        add_weak(object, &weak);
        ObjectPtr<GObject> ptr = ObjectPtr<GObject>::adopt(object);
        EXPECT_EQ(ptr.get(), object);
        EXPECT_EQ(weak, object);
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, RefKeepsObjectAliveAfterOriginalReferenceDropped) {
    void* weak = nullptr;
    {
        GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
        weak = object;
        add_weak(object, &weak);
        ObjectPtr<GObject> ptr = ObjectPtr<GObject>::ref(object);
        g_object_unref(object);
        EXPECT_EQ(ptr.get(), object);
        EXPECT_EQ(weak, object);
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, CopyThenDestroyOneCopyKeepsObjectAlive) {
    void* weak = nullptr;
    {
        GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
        weak = object;
        add_weak(object, &weak);
        ObjectPtr<GObject> first = ObjectPtr<GObject>::adopt(object);
        ObjectPtr<GObject> second(first);
        first.reset();
        EXPECT_EQ(weak, object);
        second.reset();
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, MoveLeavesSourceNullAndObjectAlive) {
    void* weak = nullptr;
    {
        GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
        weak = object;
        add_weak(object, &weak);
        ObjectPtr<GObject> source = ObjectPtr<GObject>::adopt(object);
        ObjectPtr<GObject> target(std::move(source));
        EXPECT_EQ(source.get(), nullptr);
        EXPECT_FALSE(static_cast<bool>(source));
        EXPECT_EQ(target.get(), object);
        EXPECT_EQ(weak, object);
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, ReleaseGivesPointerBackWithoutUnref) {
    void* weak = nullptr;
    GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
    weak = object;
    add_weak(object, &weak);
    ObjectPtr<GObject> ptr = ObjectPtr<GObject>::adopt(object);
    GObject* raw = ptr.release();
    EXPECT_EQ(raw, object);
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(weak, object);
    g_object_unref(raw);
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, ResetFinalizes) {
    void* weak = nullptr;
    GObject* object = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
    weak = object;
    add_weak(object, &weak);
    ObjectPtr<GObject> ptr = ObjectPtr<GObject>::adopt(object);
    ptr.reset();
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, AdoptFloatingSinksFloatingReference) {
    void* weak = nullptr;
    {
        GInitiallyUnowned* object =
            static_cast<GInitiallyUnowned*>(g_object_new(G_TYPE_INITIALLY_UNOWNED, nullptr));
        weak = object;
        add_weak(object, &weak);
        ObjectPtr<GInitiallyUnowned> ptr = ObjectPtr<GInitiallyUnowned>::adopt_floating(object);
        EXPECT_EQ(ptr.get(), object);
        EXPECT_EQ(weak, object);
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxObjectPtr, DefaultConstructedIsFalseAndSafeToDestroy) {
    ObjectPtr<GObject> ptr;
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_FALSE(static_cast<bool>(ptr));
    ptr.reset();
    SUCCEED();
}
