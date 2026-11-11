// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for gtkx::AsyncGuard: tickets find their owner, and nothing once the owner is gone.

#include <gtest/gtest.h>

#include <glib.h>

#include <memory>

#include "ui/gtkx/async_guard.h"

namespace {

using vermilion::gtkx::AsyncGuard;

struct Owner {
    int value = 7;
    AsyncGuard<Owner> guard{this};
};

TEST(GtkxAsyncGuard, TicketFindsLiveOwner) {
    Owner owner;
    gpointer ticket = owner.guard.ticket();
    Owner* found = AsyncGuard<Owner>::take(ticket);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 7);
}

TEST(GtkxAsyncGuard, TicketOutlivesOwner) {
    auto owner = std::make_unique<Owner>();
    gpointer ticket = owner->guard.ticket();
    owner.reset();
    EXPECT_EQ(AsyncGuard<Owner>::take(ticket), nullptr);
}

TEST(GtkxAsyncGuard, PeekDoesNotTakeTheTicket) {
    auto owner = std::make_unique<Owner>();
    gpointer ticket = owner->guard.ticket();
    EXPECT_EQ(AsyncGuard<Owner>::peek(ticket), owner.get());
    EXPECT_EQ(AsyncGuard<Owner>::peek(ticket), owner.get());
    owner.reset();
    EXPECT_EQ(AsyncGuard<Owner>::peek(ticket), nullptr);
    AsyncGuard<Owner>::free_ticket(ticket);
}

TEST(GtkxAsyncGuard, TicketsAreIndependent) {
    Owner owner;
    gpointer first = owner.guard.ticket();
    gpointer second = owner.guard.ticket();
    EXPECT_EQ(AsyncGuard<Owner>::take(first), &owner);
    EXPECT_EQ(AsyncGuard<Owner>::take(second), &owner);
}

} // namespace
