// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Table tests for the external-change policy: needs_hash(), decide() and kLargeFileBytes.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "core/external_change.h"
#include "core/file_stamp.h"

namespace {

using vermilion::BannerKind;
using vermilion::ChangeAction;
using vermilion::decide;
using vermilion::DiskState;
using vermilion::DocumentFacts;
using vermilion::ExternalAction;
using vermilion::FileStamp;
using vermilion::is_large;
using vermilion::kLargeFileBytes;
using vermilion::needs_hash;

constexpr std::int64_t kTime = 1000000;
constexpr std::uint64_t kSize = 50;
constexpr std::uint64_t kHashA = 0xABCDEF;
constexpr std::uint64_t kHashB = 0x123456;

FileStamp stamp() {
    FileStamp s;
    s.mtime_us = kTime;
    s.size = kSize;
    s.hash = kHashA;
    return s;
}

FileStamp copy_stamp() {
    FileStamp s;
    s.mtime_us = kTime;
    s.time_only = true;
    return s;
}

DiskState disk(std::int64_t mtime_us, std::uint64_t size) {
    DiskState d;
    d.exists = true;
    d.mtime_us = mtime_us;
    d.size = size;
    return d;
}

DiskState missing() {
    return DiskState{};
}

DocumentFacts clean() {
    return DocumentFacts{};
}

DocumentFacts edited() {
    DocumentFacts f;
    f.edited = true;
    return f;
}

DocumentFacts read_only() {
    DocumentFacts f;
    f.read_only = true;
    return f;
}

DocumentFacts asks() {
    DocumentFacts f;
    f.reload_would_ask = true;
    return f;
}

DocumentFacts deleted(bool is_edited) {
    DocumentFacts f;
    f.deleted_on_disk = true;
    f.edited = is_edited;
    return f;
}

const std::optional<std::uint64_t> kNoHash;
const std::optional<std::uint64_t> kSame = kHashA;
const std::optional<std::uint64_t> kOther = kHashB;

void expect(const ExternalAction& got, ChangeAction action, BannerKind banner) {
    EXPECT_EQ(got.action, action);
    EXPECT_EQ(got.banner, banner);
}

TEST(ExternalChangeLimit, TenMebibytes) {
    EXPECT_EQ(kLargeFileBytes, 10485760u);
    EXPECT_FALSE(is_large(0));
    EXPECT_FALSE(is_large(kLargeFileBytes));
    EXPECT_TRUE(is_large(kLargeFileBytes + 1));
}

TEST(ExternalNeedsHash, MissingFileNeedsNoHash) {
    EXPECT_FALSE(needs_hash(stamp(), missing(), false));
}

TEST(ExternalNeedsHash, SameTimeAndSizeNeedsNoHash) {
    EXPECT_FALSE(needs_hash(stamp(), disk(kTime, kSize), false));
}

TEST(ExternalNeedsHash, ADifferentTimeNeedsTheHash) {
    EXPECT_TRUE(needs_hash(stamp(), disk(kTime + 1, kSize), false));
}

TEST(ExternalNeedsHash, ADifferentSizeNeedsTheHash) {
    EXPECT_TRUE(needs_hash(stamp(), disk(kTime, kSize + 1), false));
}

TEST(ExternalNeedsHash, BothDifferentNeedsTheHash) {
    EXPECT_TRUE(needs_hash(stamp(), disk(kTime - 5, 0), false));
}

TEST(ExternalNeedsHash, AReturnedFileNeedsNoHash) {
    EXPECT_FALSE(needs_hash(stamp(), disk(kTime + 1, kSize + 1), true));
}

TEST(ExternalNeedsHash, ATimeOnlyStampWithTheSameTimeNeedsTheHash) {
    EXPECT_TRUE(needs_hash(copy_stamp(), disk(kTime, 7), false));
}

TEST(ExternalNeedsHash, ATimeOnlyStampWithAnotherTimeNeedsNoHash) {
    EXPECT_FALSE(needs_hash(copy_stamp(), disk(kTime + 1, 7), false));
}

TEST(ExternalNeedsHash, ATimeOnlyStampOfAMissingFileNeedsNoHash) {
    EXPECT_FALSE(needs_hash(copy_stamp(), missing(), false));
}

TEST(ExternalDecideMissing, ACleanDocumentIsMarkedDeleted) {
    expect(decide(stamp(), missing(), kNoHash, clean()), ChangeAction::MarkDeleted,
           BannerKind::None);
}

TEST(ExternalDecideMissing, AnEditedDocumentIsMarkedDeleted) {
    expect(decide(stamp(), missing(), kNoHash, edited()), ChangeAction::MarkDeleted,
           BannerKind::None);
}

TEST(ExternalDecideMissing, AReadOnlyDocumentIsMarkedDeleted) {
    expect(decide(stamp(), missing(), kNoHash, read_only()), ChangeAction::MarkDeleted,
           BannerKind::None);
}

TEST(ExternalDecideMissing, AlreadyMarkedDeletedIsIgnored) {
    expect(decide(stamp(), missing(), kNoHash, deleted(false)), ChangeAction::Ignore,
           BannerKind::None);
    expect(decide(stamp(), missing(), kNoHash, deleted(true)), ChangeAction::Ignore,
           BannerKind::None);
}

TEST(ExternalDecideSame, SameTimeAndSizeIsIgnoredWhateverTheHash) {
    expect(decide(stamp(), disk(kTime, kSize), kNoHash, clean()), ChangeAction::Ignore,
           BannerKind::None);
    expect(decide(stamp(), disk(kTime, kSize), kOther, clean()), ChangeAction::Ignore,
           BannerKind::None);
    expect(decide(stamp(), disk(kTime, kSize), kOther, edited()), ChangeAction::Ignore,
           BannerKind::None);
}

TEST(ExternalDecideTouched, ADifferentTimeWithoutAHashIsIgnored) {
    expect(decide(stamp(), disk(kTime + 1, kSize), kNoHash, clean()), ChangeAction::Ignore,
           BannerKind::None);
}

TEST(ExternalDecideTouched, AnEqualHashOnlyUpdatesTheStamp) {
    const DiskState touched = disk(kTime + 1, kSize);
    expect(decide(stamp(), touched, kSame, clean()), ChangeAction::UpdateStampOnly,
           BannerKind::None);
    expect(decide(stamp(), touched, kSame, edited()), ChangeAction::UpdateStampOnly,
           BannerKind::None);
    expect(decide(stamp(), touched, kSame, read_only()), ChangeAction::UpdateStampOnly,
           BannerKind::None);
    expect(decide(stamp(), touched, kSame, asks()), ChangeAction::UpdateStampOnly,
           BannerKind::None);
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes + 1), kSame, clean()),
           ChangeAction::UpdateStampOnly, BannerKind::None);
}

TEST(ExternalDecideChanged, ACleanSmallDocumentReloadsSilently) {
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, clean()),
           ChangeAction::ReloadSilently, BannerKind::None);
}

TEST(ExternalDecideChanged, AChangeWithTheSameSizeStillReloads) {
    expect(decide(stamp(), disk(kTime + 1, kSize), kOther, clean()), ChangeAction::ReloadSilently,
           BannerKind::None);
}

TEST(ExternalDecideChanged, AFileEmptiedOutsideReloadsSilently) {
    expect(decide(stamp(), disk(kTime + 1, 0), kOther, clean()), ChangeAction::ReloadSilently,
           BannerKind::None);
}

TEST(ExternalDecideChanged, ExactlyTheLimitStillReloadsSilently) {
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes), kOther, clean()),
           ChangeAction::ReloadSilently, BannerKind::None);
}

TEST(ExternalDecideChanged, OneByteOverTheLimitShowsTheLargeBanner) {
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes + 1), kOther, clean()),
           ChangeAction::ShowBanner, BannerKind::ChangedLarge);
}

TEST(ExternalDecideChanged, TheSizeTestedIsTheNewSizeOnDisk) {
    FileStamp big = stamp();
    big.size = kLargeFileBytes * 2;
    expect(decide(big, disk(kTime + 1, kSize), kOther, clean()), ChangeAction::ReloadSilently,
           BannerKind::None);
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes * 2), kOther, clean()),
           ChangeAction::ShowBanner, BannerKind::ChangedLarge);
}

TEST(ExternalDecideChanged, AnEditedDocumentAlwaysGetsTheChangedBanner) {
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, edited()), ChangeAction::ShowBanner,
           BannerKind::Changed);
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes + 1), kOther, edited()),
           ChangeAction::ShowBanner, BannerKind::Changed);
    expect(decide(stamp(), disk(kTime + 1, 0), kOther, edited()), ChangeAction::ShowBanner,
           BannerKind::Changed);
}

TEST(ExternalDecideChanged, AReloadThatWouldAskGetsTheChangedBanner) {
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, asks()), ChangeAction::ShowBanner,
           BannerKind::Changed);
}

TEST(ExternalDecideChanged, TheSizeRuleComesBeforeTheQuestionRule) {
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes + 1), kOther, asks()),
           ChangeAction::ShowBanner, BannerKind::ChangedLarge);
}

TEST(ExternalDecideChanged, AnEditedDocumentThatWouldAskGetsTheChangedBanner) {
    DocumentFacts both = edited();
    both.reload_would_ask = true;
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, both), ChangeAction::ShowBanner,
           BannerKind::Changed);
}

TEST(ExternalDecideReadOnly, FollowsTheSameSizeRule) {
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, read_only()),
           ChangeAction::ReloadSilently, BannerKind::None);
    expect(decide(stamp(), disk(kTime + 1, kLargeFileBytes + 1), kOther, read_only()),
           ChangeAction::ShowBanner, BannerKind::ChangedLarge);
}

TEST(ExternalDecideReadOnly, AReloadThatWouldAskGetsTheChangedBanner) {
    DocumentFacts facts = read_only();
    facts.reload_would_ask = true;
    expect(decide(stamp(), disk(kTime + 1, kSize + 3), kOther, facts), ChangeAction::ShowBanner,
           BannerKind::Changed);
}

TEST(ExternalDecideReturned, ACleanSmallDocumentReloadsSilently) {
    expect(decide(stamp(), disk(kTime + 9, kSize + 9), kNoHash, deleted(false)),
           ChangeAction::ReloadSilently, BannerKind::None);
}

TEST(ExternalDecideReturned, TheDeletedFlagBeatsEqualTimeAndSize) {
    expect(decide(stamp(), disk(kTime, kSize), kNoHash, deleted(false)),
           ChangeAction::ReloadSilently, BannerKind::None);
}

TEST(ExternalDecideReturned, AnEditedDocumentGetsTheReturnedBanner) {
    expect(decide(stamp(), disk(kTime + 9, kSize + 9), kNoHash, deleted(true)),
           ChangeAction::ShowBanner, BannerKind::Returned);
}

TEST(ExternalDecideReturned, ALargeFileGetsTheReturnedBanner) {
    expect(decide(stamp(), disk(kTime + 9, kLargeFileBytes + 1), kNoHash, deleted(false)),
           ChangeAction::ShowBanner, BannerKind::Returned);
}

TEST(ExternalDecideReturned, AReloadThatWouldAskGetsTheReturnedBanner) {
    DocumentFacts facts = deleted(false);
    facts.reload_would_ask = true;
    expect(decide(stamp(), disk(kTime + 9, kSize), kNoHash, facts), ChangeAction::ShowBanner,
           BannerKind::Returned);
}

TEST(ExternalDecideTimeOnly, ADifferentTimeIsAChange) {
    expect(decide(copy_stamp(), disk(kTime + 1, 7), kNoHash, edited()), ChangeAction::ShowBanner,
           BannerKind::Changed);
}

TEST(ExternalDecideTimeOnly, ADifferentTimeOnACleanDocumentFollowsTheChangeRules) {
    expect(decide(copy_stamp(), disk(kTime - 1, 7), kNoHash, clean()), ChangeAction::ReloadSilently,
           BannerKind::None);
    expect(decide(copy_stamp(), disk(kTime + 1, kLargeFileBytes + 1), kNoHash, clean()),
           ChangeAction::ShowBanner, BannerKind::ChangedLarge);
}

TEST(ExternalDecideTimeOnly, TheSameTimeFillsTheStampWhenTheHashIsKnown) {
    expect(decide(copy_stamp(), disk(kTime, 7), kOther, edited()), ChangeAction::UpdateStampOnly,
           BannerKind::None);
}

TEST(ExternalDecideTimeOnly, TheSameTimeWithoutAHashIsIgnored) {
    expect(decide(copy_stamp(), disk(kTime, 7), kNoHash, edited()), ChangeAction::Ignore,
           BannerKind::None);
}

TEST(ExternalDecideTimeOnly, AMissingFileIsMarkedDeleted) {
    expect(decide(copy_stamp(), missing(), kNoHash, edited()), ChangeAction::MarkDeleted,
           BannerKind::None);
}

TEST(ExternalDecideDefaults, AnActionStartsAsIgnore) {
    const ExternalAction none;
    EXPECT_EQ(none.action, ChangeAction::Ignore);
    EXPECT_EQ(none.banner, BannerKind::None);
}

} // namespace
