// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for gtkx::connect and SignalConnection, using GListStore and GSimpleAction signals.

#include <gtest/gtest.h>

#include <gio/gio.h>

#include <memory>

#include "ui/gtkx/object_ptr.h"
#include "ui/gtkx/signal.h"

using vermilion::gtkx::ObjectPtr;
using vermilion::gtkx::SignalConnection;
using vermilion::gtkx::connect;

TEST(GtkxSignal, ListStoreItemsChangedReportsPositionRemovedAdded) {
    ObjectPtr<GListStore> store = ObjectPtr<GListStore>::adopt(
        static_cast<GListStore*>(g_list_store_new(G_TYPE_OBJECT)));
    GListModel* raw = G_LIST_MODEL(store.get());

    guint calls = 0;
    guint position = 999;
    guint removed = 999;
    guint added = 999;
    GListModel* seen_model = nullptr;

    SignalConnection conn = connect<void(GListModel*, guint, guint, guint)>(
        raw, "items-changed",
        [&](GListModel* model, guint pos, guint rem, guint add) {
            ++calls;
            seen_model = model;
            position = pos;
            removed = rem;
            added = add;
        });
    ASSERT_TRUE(static_cast<bool>(conn));

    GObject* item = static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr));
    g_list_store_append(store.get(), item);
    g_object_unref(item);

    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(position, 0u);
    EXPECT_EQ(removed, 0u);
    EXPECT_EQ(added, 1u);
    EXPECT_EQ(seen_model, raw);

    g_list_store_remove(store.get(), 0);

    EXPECT_EQ(calls, 2u);
    EXPECT_EQ(position, 0u);
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(added, 0u);
}

TEST(GtkxSignal, ActionActivateDeliversParameter) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", G_VARIANT_TYPE_INT32)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int value = -1;
    GSimpleAction* seen_action = nullptr;

    SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction* a, GVariant* parameter) {
            seen_action = a;
            if (parameter != nullptr) {
                value = g_variant_get_int32(parameter);
            }
        });
    ASSERT_TRUE(static_cast<bool>(conn));

    g_action_activate(G_ACTION(raw), g_variant_new_int32(42));

    EXPECT_EQ(value, 42);
    EXPECT_EQ(seen_action, raw);
}

TEST(GtkxSignal, ActionActivateWithoutParameter) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int called = 0;
    GVariant* seen = reinterpret_cast<GVariant*>(1);

    SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant* parameter) {
            ++called;
            seen = parameter;
        });
    ASSERT_TRUE(static_cast<bool>(conn));

    g_action_activate(G_ACTION(raw), nullptr);

    EXPECT_EQ(called, 1);
    EXPECT_EQ(seen, nullptr);
}

TEST(GtkxSignal, DestructorDisconnects) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int count = 0;
    {
        SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
            raw, "activate",
            [&](GSimpleAction*, GVariant*) { ++count; });
        ASSERT_TRUE(static_cast<bool>(conn));
        g_action_activate(G_ACTION(raw), nullptr);
        EXPECT_EQ(count, 1);
    }
    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(count, 1);
}

TEST(GtkxSignal, ExplicitDisconnectStops) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int count = 0;
    SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant*) { ++count; });
    ASSERT_TRUE(static_cast<bool>(conn));

    conn.disconnect();
    EXPECT_FALSE(static_cast<bool>(conn));
    conn.disconnect();
    EXPECT_FALSE(static_cast<bool>(conn));

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(count, 0);
}

TEST(GtkxSignal, MoveTransfersOwnership) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int first = 0;
    int second = 0;

    SignalConnection source = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant*) { ++first; });
    ASSERT_TRUE(static_cast<bool>(source));

    SignalConnection target(std::move(source));
    EXPECT_FALSE(static_cast<bool>(source));
    EXPECT_TRUE(static_cast<bool>(target));

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(first, 1);

    {
        SignalConnection dead(std::move(source));
        (void)dead;
    }
    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(first, 2);

    SignalConnection other = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant*) { ++second; });
    ASSERT_TRUE(static_cast<bool>(other));

    target = std::move(other);
    EXPECT_FALSE(static_cast<bool>(other));
    EXPECT_TRUE(static_cast<bool>(target));

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(first, 2);
    EXPECT_EQ(second, 1);
}

TEST(GtkxSignal, DetachKeepsHandlerConnected) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int count = 0;
    SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant*) { ++count; });
    ASSERT_TRUE(static_cast<bool>(conn));

    conn.detach();
    EXPECT_FALSE(static_cast<bool>(conn));

    {
        SignalConnection dead(std::move(conn));
        (void)dead;
    }

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(count, 1);
}

TEST(GtkxSignal, InstanceFinalizedFirstIsSafe) {
    void* weak = nullptr;
    ObjectPtr<GSimpleAction> action;
    {
        action = ObjectPtr<GSimpleAction>::adopt(
            static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
        GSimpleAction* raw = G_SIMPLE_ACTION(action.get());
        weak = raw;
        g_object_add_weak_pointer(G_OBJECT(raw), &weak);

        SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
            raw, "activate",
            [](GSimpleAction*, GVariant*) {});
        ASSERT_TRUE(static_cast<bool>(conn));

        action.reset();
        EXPECT_EQ(weak, nullptr);

        conn.disconnect();
        EXPECT_FALSE(static_cast<bool>(conn));
        conn.disconnect();
    }
    EXPECT_EQ(weak, nullptr);
}

TEST(GtkxSignal, ClosureIsFreedWhenInstanceDies) {
    std::shared_ptr<int> captured = std::make_shared<int>(7);
    EXPECT_EQ(captured.use_count(), 1u);

    ObjectPtr<GSimpleAction> action;
    SignalConnection conn;
    {
        action = ObjectPtr<GSimpleAction>::adopt(
            static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
        GSimpleAction* raw = G_SIMPLE_ACTION(action.get());
        conn = connect<void(GSimpleAction*, GVariant*)>(
            raw, "activate",
            [captured](GSimpleAction*, GVariant*) {
                *captured += 1;
            });
        ASSERT_TRUE(static_cast<bool>(conn));
        EXPECT_EQ(captured.use_count(), 2u);

        action.reset();
    }
    EXPECT_EQ(captured.use_count(), 1u);
}

TEST(GtkxSignal, ClosureIsFreedOnDisconnect) {
    std::shared_ptr<int> captured = std::make_shared<int>(7);
    EXPECT_EQ(captured.use_count(), 1u);

    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    SignalConnection conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [captured](GSimpleAction*, GVariant*) {
            *captured += 1;
        });
    ASSERT_TRUE(static_cast<bool>(conn));
    EXPECT_EQ(captured.use_count(), 2u);

    conn.disconnect();
    EXPECT_EQ(captured.use_count(), 1u);
}

TEST(GtkxSignal, DefaultConstructedIsInactiveAndSafe) {
    SignalConnection conn;
    EXPECT_FALSE(static_cast<bool>(conn));
    conn.disconnect();
    EXPECT_FALSE(static_cast<bool>(conn));
    conn.detach();
    EXPECT_FALSE(static_cast<bool>(conn));
    SUCCEED();
}

TEST(GtkxSignal, DisconnectInsideOwnHandler) {
    ObjectPtr<GSimpleAction> action = ObjectPtr<GSimpleAction>::adopt(
        static_cast<GSimpleAction*>(g_simple_action_new("test", nullptr)));
    GSimpleAction* raw = G_SIMPLE_ACTION(action.get());

    int count = 0;
    SignalConnection conn;
    conn = connect<void(GSimpleAction*, GVariant*)>(
        raw, "activate",
        [&](GSimpleAction*, GVariant*) { ++count; conn.disconnect(); });
    ASSERT_TRUE(static_cast<bool>(conn));

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(static_cast<bool>(conn));

    g_action_activate(G_ACTION(raw), nullptr);
    EXPECT_EQ(count, 1);
}
