// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the start-up list of recovery copies: the scan, its order, and the walk through it.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/recovery_file.h"
#include "core/recovery_offer.h"
#include "core/recovery_store.h"
#include "recovery_store_test_util.h"

namespace {

using store_test::put;
using store_test::RecoveryStoreTest;
using vermilion::RecoveryHeader;
using vermilion::RecoveryItem;
using vermilion::RecoveryOffer;
using vermilion::RecoveryProgram;
using vermilion::RecoveryStore;
using Ids = std::vector<std::string>;

constexpr const char* kA = "aaaaaaaa-0000-4000-8000-000000000001";
constexpr const char* kB = "bbbbbbbb-0000-4000-8000-000000000002";
constexpr const char* kC = "cccccccc-0000-4000-8000-000000000003";

RecoveryHeader titled(const std::string& title, std::int64_t copied) {
    RecoveryHeader header;
    header.title = title;
    header.copied_us = copied;
    return header;
}

Ids ids_of(const std::vector<RecoveryItem>& items) {
    Ids ids;
    for (const RecoveryItem& item : items) {
        ids.push_back(item.id);
    }
    return ids;
}

RecoveryItem item(const std::string& id, bool damaged = false) {
    RecoveryItem result;
    result.id = id;
    result.damaged = damaged;
    return result;
}

TEST_F(RecoveryStoreTest, AFolderThatIsNotThereGivesNoItems) {
    EXPECT_TRUE(vermilion::scan_recovery(light()).empty());
    RecoveryStore none("", RecoveryProgram::Light);
    EXPECT_TRUE(vermilion::scan_recovery(none).empty());
}

TEST_F(RecoveryStoreTest, ValidCopiesComeOldestFirstAndEqualTimesInIdOrder) {
    RecoveryStore store = light();
    ASSERT_TRUE(store.write(kC, titled("c", 100), "c").is_ok());
    ASSERT_TRUE(store.write(kA, titled("a", 300), "a").is_ok());
    ASSERT_TRUE(store.write(kB, titled("b", 100), "b").is_ok());
    EXPECT_EQ(ids_of(vermilion::scan_recovery(store)), Ids({kB, kC, kA}));
}

TEST_F(RecoveryStoreTest, DamagedCopiesFollowInIdOrderWithTheirReason) {
    RecoveryStore store = light();
    ASSERT_TRUE(store.write(kB, titled("b", 200), "b").is_ok());
    make_folder();
    put(in_folder("zzz-bad.vrec"), "not a recovery file");
    put(in_folder("ccc-bad.vrec"), "");
    ASSERT_TRUE(store.write(kA, titled("a", 100), "a").is_ok());
    std::vector<RecoveryItem> items = vermilion::scan_recovery(store);
    EXPECT_EQ(ids_of(items), Ids({kA, kB, "ccc-bad", "zzz-bad"}));
    EXPECT_FALSE(items[0].damaged);
    EXPECT_FALSE(items[1].damaged);
    EXPECT_TRUE(items[2].damaged);
    EXPECT_TRUE(items[3].damaged);
    EXPECT_FALSE(items[2].problem.empty());
    EXPECT_FALSE(items[3].problem.empty());
}

TEST_F(RecoveryStoreTest, AValidItemCarriesTheHeaderAndNoText) {
    RecoveryStore store = light();
    RecoveryHeader header;
    header.uri = "file:///home/adam/a.txt";
    header.copied_us = 777;
    header.file_mtime_us = 555;
    ASSERT_TRUE(store.write(kA, header, "the text").is_ok());
    std::vector<RecoveryItem> items = vermilion::scan_recovery(store);
    ASSERT_EQ(items.size(), 1u);
    EXPECT_FALSE(items[0].damaged);
    EXPECT_TRUE(items[0].problem.empty());
    EXPECT_EQ(items[0].header.uri, std::optional<std::string>("file:///home/adam/a.txt"));
    EXPECT_EQ(items[0].header.copied_us, 777);
    EXPECT_EQ(items[0].header.file_mtime_us, std::optional<std::int64_t>(555));
    EXPECT_EQ(items[0].header.length, 8u);
}

TEST_F(RecoveryStoreTest, OtherFilesInTheFolderAreNotItems) {
    RecoveryStore store = light();
    ASSERT_TRUE(store.write(kA, titled("a", 1), "a").is_ok());
    put(in_folder("notes.txt"), "x");
    put(in_folder(".vrec-tmp-abc123"), "x");
    put(in_folder("UPPER.vrec"), "x");
    EXPECT_EQ(ids_of(vermilion::scan_recovery(store)), Ids({kA}));
}

TEST(RecoveryOffer, AnEmptyOfferIsFinished) {
    RecoveryOffer offer({});
    EXPECT_TRUE(offer.finished());
    EXPECT_EQ(offer.total(), 0u);
    EXPECT_EQ(offer.current(), nullptr);
    EXPECT_EQ(offer.number(), 0u);
    EXPECT_FALSE(offer.can_recover_all());
    offer.answered();
    offer.decide_later();
    EXPECT_TRUE(offer.take_valid().empty());
    EXPECT_EQ(offer.kept(), 0u);
}

TEST(RecoveryOffer, AnswersWalkTheCopiesInOrderWithTheirNumbers) {
    RecoveryOffer offer({item("a"), item("b"), item("c")});
    EXPECT_EQ(offer.total(), 3u);
    ASSERT_TRUE(offer.current() != nullptr);
    EXPECT_EQ(offer.current()->id, "a");
    EXPECT_EQ(offer.number(), 1u);
    offer.answered();
    EXPECT_EQ(offer.current()->id, "b");
    EXPECT_EQ(offer.number(), 2u);
    offer.answered();
    EXPECT_EQ(offer.current()->id, "c");
    EXPECT_EQ(offer.number(), 3u);
    EXPECT_FALSE(offer.finished());
    offer.answered();
    EXPECT_TRUE(offer.finished());
    EXPECT_EQ(offer.current(), nullptr);
    EXPECT_EQ(offer.number(), 0u);
    EXPECT_EQ(offer.total(), 3u);
    EXPECT_EQ(offer.kept(), 0u);
}

TEST(RecoveryOffer, DecideLaterKeepsTheCopyAndTheNumbering) {
    RecoveryOffer offer({item("a"), item("b"), item("c")});
    offer.decide_later();
    EXPECT_EQ(offer.kept(), 1u);
    EXPECT_EQ(offer.current()->id, "b");
    EXPECT_EQ(offer.number(), 2u);
    offer.answered();
    offer.decide_later();
    EXPECT_EQ(offer.kept(), 2u);
    EXPECT_TRUE(offer.finished());
    offer.decide_later();
    EXPECT_EQ(offer.kept(), 2u);
}

TEST(RecoveryOffer, RecoverAllNeedsAValidCurrentCopyAndTwoValidOnesLeft) {
    RecoveryOffer one({item("a")});
    EXPECT_FALSE(one.can_recover_all());
    RecoveryOffer two({item("a"), item("b")});
    EXPECT_TRUE(two.can_recover_all());
    two.decide_later();
    EXPECT_FALSE(two.can_recover_all());
    RecoveryOffer damaged_last({item("a"), item("b", true)});
    EXPECT_FALSE(damaged_last.can_recover_all());
    RecoveryOffer damaged_first({item("a", true), item("b"), item("c")});
    EXPECT_FALSE(damaged_first.can_recover_all());
    damaged_first.answered();
    EXPECT_TRUE(damaged_first.can_recover_all());
}

TEST(RecoveryOffer, TakeValidReturnsTheValidCopiesFromTheCurrentOneOn) {
    RecoveryOffer offer({item("a"), item("b"), item("c"), item("d", true), item("e", true)});
    offer.answered();
    std::vector<RecoveryItem> taken = offer.take_valid();
    EXPECT_EQ(ids_of(taken), Ids({"b", "c"}));
    EXPECT_TRUE(offer.finished());
    EXPECT_EQ(offer.kept(), 2u);
    EXPECT_EQ(offer.number(), 0u);
    EXPECT_TRUE(offer.take_valid().empty());
}

TEST(RecoveryOffer, TakeValidKeepsTheEarlierDecideLaterCount) {
    RecoveryOffer offer({item("a"), item("b"), item("c", true)});
    offer.decide_later();
    EXPECT_EQ(ids_of(offer.take_valid()), Ids({"b"}));
    EXPECT_EQ(offer.kept(), 2u);
}

} // namespace
