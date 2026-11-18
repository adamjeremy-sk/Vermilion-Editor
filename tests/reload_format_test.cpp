// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the format a document takes when it is reloaded from its file.

#include <gtest/gtest.h>

#include "core/reload_format.h"
#include "core/text_format.h"
#include "core/text_scan.h"

namespace {

using vermilion::LineEnding;
using vermilion::reload_format;
using vermilion::ReloadFormat;
using vermilion::ScanResult;
using vermilion::TextEncoding;
using vermilion::TextFormat;

TextFormat format_of(TextEncoding encoding, bool bom, LineEnding ending) {
    TextFormat format;
    format.encoding = encoding;
    format.bom = bom;
    format.line_ending = ending;
    return format;
}

ScanResult scan_of(TextEncoding encoding, bool bom, LineEnding ending, bool mixed) {
    ScanResult scan;
    scan.format = format_of(encoding, bom, ending);
    scan.mixed_line_endings = mixed;
    return scan;
}

TEST(ReloadFormat, WithoutAChosenEncodingTheScanDecidesEverything) {
    const TextFormat current = format_of(TextEncoding::Utf8, false, LineEnding::Lf);
    const ScanResult scan = scan_of(TextEncoding::Windows1252, false, LineEnding::CrLf, true);
    ReloadFormat result = reload_format(current, false, false, scan);
    EXPECT_TRUE(result.format.encoding == TextEncoding::Windows1252);
    EXPECT_TRUE(result.format.line_ending == LineEnding::CrLf);
    EXPECT_FALSE(result.format.bom);
    EXPECT_TRUE(result.mixed_line_endings);
}

TEST(ReloadFormat, WithoutAChosenEncodingAByteOrderMarkInTheScanIsKept) {
    const TextFormat current = format_of(TextEncoding::Utf8, false, LineEnding::Lf);
    const ScanResult scan = scan_of(TextEncoding::Utf8, true, LineEnding::Lf, false);
    EXPECT_TRUE(reload_format(current, false, false, scan).format.bom);
}

TEST(ReloadFormat, WithoutAChosenEncodingTheCurrentFlagsAreForgotten) {
    const TextFormat current = format_of(TextEncoding::Latin1, false, LineEnding::Cr);
    const ScanResult scan = scan_of(TextEncoding::Utf8, false, LineEnding::Lf, false);
    ReloadFormat result = reload_format(current, true, false, scan);
    EXPECT_TRUE(result.format.line_ending == LineEnding::Lf);
    EXPECT_FALSE(result.mixed_line_endings);
}

TEST(ReloadFormat, AChosenEncodingIsKeptWhateverTheScanGuessed) {
    const TextFormat current = format_of(TextEncoding::Windows1252, false, LineEnding::Lf);
    const ScanResult scan = scan_of(TextEncoding::Utf8, false, LineEnding::Lf, false);
    EXPECT_TRUE(reload_format(current, false, true, scan).format.encoding ==
                TextEncoding::Windows1252);
}

TEST(ReloadFormat, AChosenEncodingKeepsItsByteOrderMarkSetting) {
    const TextFormat current = format_of(TextEncoding::Utf16Le, true, LineEnding::Lf);
    const ScanResult scan = scan_of(TextEncoding::Utf8, false, LineEnding::Lf, false);
    EXPECT_TRUE(reload_format(current, false, true, scan).format.bom);
}

TEST(ReloadFormat, AChosenEncodingTakesTheLineEndingsOfAScanThatAgrees) {
    const TextFormat current = format_of(TextEncoding::Utf8, false, LineEnding::Lf);
    const ScanResult scan = scan_of(TextEncoding::Utf8, false, LineEnding::CrLf, true);
    ReloadFormat result = reload_format(current, false, true, scan);
    EXPECT_TRUE(result.format.line_ending == LineEnding::CrLf);
    EXPECT_TRUE(result.mixed_line_endings);
}

TEST(ReloadFormat, AChosenEncodingKeepsTheLineEndingsWhenTheScanDisagrees) {
    const TextFormat current = format_of(TextEncoding::Latin1, false, LineEnding::Cr);
    const ScanResult scan = scan_of(TextEncoding::Utf8, false, LineEnding::Lf, false);
    ReloadFormat result = reload_format(current, true, true, scan);
    EXPECT_TRUE(result.format.encoding == TextEncoding::Latin1);
    EXPECT_TRUE(result.format.line_ending == LineEnding::Cr);
    EXPECT_TRUE(result.mixed_line_endings);
}

} // namespace
