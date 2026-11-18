// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Table tests for the read-only look at a file: classify_look() and close_case().

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "core/external_change.h"
#include "core/file_stamp.h"

namespace {

using vermilion::classify_look;
using vermilion::close_case;
using vermilion::CloseCase;
using vermilion::DiskState;
using vermilion::FileLook;
using vermilion::FileStamp;

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

const std::optional<std::uint64_t> kNoHash;

TEST(ClassifyLook, AMissingFileIsGone) {
    EXPECT_EQ(classify_look(stamp(), DiskState{}, kNoHash, false), FileLook::Gone);
    EXPECT_EQ(classify_look(stamp(), DiskState{}, kHashA, true), FileLook::Gone);
    EXPECT_EQ(classify_look(copy_stamp(), DiskState{}, kNoHash, false), FileLook::Gone);
}

TEST(ClassifyLook, AFileThatCameBackIsChanged) {
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize), kNoHash, true), FileLook::Changed);
    EXPECT_EQ(classify_look(stamp(), disk(kTime + 1, kSize + 1), kHashA, true), FileLook::Changed);
}

TEST(ClassifyLook, TheSameTimeAndSizeIsTheSameWithoutAHash) {
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize), kNoHash, false), FileLook::Same);
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize), kHashB, false), FileLook::Same);
}

TEST(ClassifyLook, ANewTimeOrSizeIsDecidedByTheHash) {
    EXPECT_EQ(classify_look(stamp(), disk(kTime + 5, kSize), kHashA, false), FileLook::Same);
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize + 1), kHashA, false), FileLook::Same);
    EXPECT_EQ(classify_look(stamp(), disk(kTime + 5, kSize), kHashB, false), FileLook::Changed);
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize + 1), kHashB, false), FileLook::Changed);
}

TEST(ClassifyLook, ANewTimeOrSizeWithoutAHashIsUnknown) {
    EXPECT_EQ(classify_look(stamp(), disk(kTime + 5, kSize), kNoHash, false), FileLook::Unknown);
    EXPECT_EQ(classify_look(stamp(), disk(kTime, kSize + 1), kNoHash, false), FileLook::Unknown);
}

TEST(ClassifyLook, AStampWithoutAHashCannotBeEqual) {
    FileStamp s = stamp();
    s.hash.reset();
    EXPECT_EQ(classify_look(s, disk(kTime + 5, kSize), kHashA, false), FileLook::Changed);
    EXPECT_EQ(classify_look(s, disk(kTime, kSize), kNoHash, false), FileLook::Same);
}

TEST(ClassifyLook, ATimeOnlyStampFollowsTheTime) {
    EXPECT_EQ(classify_look(copy_stamp(), disk(kTime + 1, kSize), kNoHash, false),
              FileLook::Changed);
    EXPECT_EQ(classify_look(copy_stamp(), disk(kTime, kSize), kHashA, false), FileLook::Same);
    EXPECT_EQ(classify_look(copy_stamp(), disk(kTime, kSize), kNoHash, false), FileLook::Unknown);
}

TEST(CloseCaseTest, AnEditedDocumentWhoseFileIsUnchangedGetsTheUsualQuestion) {
    EXPECT_EQ(close_case(true, false, FileLook::Same), CloseCase::Edited);
    EXPECT_EQ(close_case(true, false, FileLook::Unknown), CloseCase::Edited);
}

TEST(CloseCaseTest, AnEditedDocumentWhoseFileChangedGetsTheExtraLine) {
    EXPECT_EQ(close_case(true, false, FileLook::Changed), CloseCase::EditedFileChanged);
}

TEST(CloseCaseTest, ACleanDocumentWhoseFileChangedStaysTheUsualCase) {
    EXPECT_EQ(close_case(false, false, FileLook::Changed), CloseCase::Edited);
}

TEST(CloseCaseTest, AGoneFileGetsTheGoneQuestion) {
    EXPECT_EQ(close_case(false, false, FileLook::Gone), CloseCase::FileGone);
    EXPECT_EQ(close_case(true, false, FileLook::Gone), CloseCase::FileGone);
}

TEST(CloseCaseTest, ADocumentMarkedDeletedIsGoneUnlessTheFileCameBack) {
    EXPECT_EQ(close_case(false, true, FileLook::Gone), CloseCase::FileGone);
    EXPECT_EQ(close_case(true, true, FileLook::Gone), CloseCase::FileGone);
    EXPECT_EQ(close_case(false, true, FileLook::Unknown), CloseCase::FileGone);
    EXPECT_EQ(close_case(true, true, FileLook::Same), CloseCase::FileGone);
    EXPECT_EQ(close_case(false, true, FileLook::Changed), CloseCase::EditedFileChanged);
    EXPECT_EQ(close_case(true, true, FileLook::Changed), CloseCase::EditedFileChanged);
}

} // namespace
