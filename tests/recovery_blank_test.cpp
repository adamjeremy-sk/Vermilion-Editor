// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the check that the start-up blank document is still untouched.

#include <gtest/gtest.h>

#include "core/document_list.h"
#include "core/recovery_blank.h"

namespace {

using vermilion::DocumentRecord;

DocumentRecord blank() {
    DocumentRecord record;
    record.id = 1;
    record.title = "Untitled 1";
    record.untitled_number = 1;
    return record;
}

TEST(RecoveryBlank, AnEmptyUntouchedUntitledDocumentIsBlank) {
    EXPECT_TRUE(vermilion::is_untouched_blank(blank(), 0));
}

TEST(RecoveryBlank, TextMakesItNotBlank) {
    EXPECT_FALSE(vermilion::is_untouched_blank(blank(), 1));
    EXPECT_FALSE(vermilion::is_untouched_blank(blank(), 5000));
}

TEST(RecoveryBlank, AnEditMakesItNotBlankEvenWhenTheTextIsEmptyAgain) {
    DocumentRecord record = blank();
    record.modified = true;
    EXPECT_FALSE(vermilion::is_untouched_blank(record, 0));
}

TEST(RecoveryBlank, AFileIsNeverBlank) {
    DocumentRecord record = blank();
    record.path = "/home/adam/empty.txt";
    record.untitled_number = 0;
    EXPECT_FALSE(vermilion::is_untouched_blank(record, 0));
}

TEST(RecoveryBlank, AReadOnlyDocumentIsNeverBlank) {
    DocumentRecord record = blank();
    record.read_only = true;
    EXPECT_FALSE(vermilion::is_untouched_blank(record, 0));
}

} // namespace
