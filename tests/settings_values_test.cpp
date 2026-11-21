// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the integer and text values of SettingsFile: reads, writes, escapes, two writers.

#include <gtest/gtest.h>

#include <string>

#include "core/result.h"
#include "core/settings_file.h"
#include "settings_test_dir.h"

namespace {

using settings_test::SettingsDir;
using vermilion::SettingsFile;
using vermilion::SettingsLoad;

class SettingsValuesTest : public SettingsDir {};

TEST_F(SettingsValuesTest, AMissingIntegerGivesTheFallback) {
    SettingsFile settings(path_);
    settings.load();
    EXPECT_EQ(settings.get_int("editor", "tab_width", 4), 4);
    EXPECT_EQ(settings.get_int("editor", "tab_width", 8), 8);
}

TEST_F(SettingsValuesTest, AnIntegerIsVisibleBeforeSave) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_int("editor", "tab_width", 8);
    EXPECT_EQ(settings.get_int("editor", "tab_width", 4), 8);
    EXPECT_FALSE(file_exists());
}

TEST_F(SettingsValuesTest, AnIntegerReadsBackAfterSave) {
    SettingsFile writer(path_);
    writer.load();
    writer.set_int("files", "long_line_limit", 500000);
    ASSERT_TRUE(writer.save().is_ok());
    EXPECT_TRUE(contains(text(), "long_line_limit=500000"));

    SettingsFile reader(path_);
    EXPECT_EQ(reader.load(), SettingsLoad::Loaded);
    EXPECT_EQ(reader.get_int("files", "long_line_limit", 1), 500000);
}

TEST_F(SettingsValuesTest, ANegativeIntegerAndZeroRoundTrip) {
    SettingsFile writer(path_);
    writer.load();
    writer.set_int("t", "minus", -42);
    writer.set_int("t", "zero", 0);
    ASSERT_TRUE(writer.save().is_ok());

    SettingsFile reader(path_);
    reader.load();
    EXPECT_EQ(reader.get_int("t", "minus", 7), -42);
    EXPECT_EQ(reader.get_int("t", "zero", 7), 0);
}

TEST_F(SettingsValuesTest, ATextThatIsNotAWholeNumberGivesTheFallback) {
    put("[t]\nword=abc\nfraction=1.5\nempty=\ntrailing=12abc\nhuge=99999999999\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_EQ(settings.get_int("t", "word", 7), 7);
    EXPECT_EQ(settings.get_int("t", "fraction", 7), 7);
    EXPECT_EQ(settings.get_int("t", "empty", 7), 7);
    EXPECT_EQ(settings.get_int("t", "trailing", 7), 7);
    EXPECT_EQ(settings.get_int("t", "huge", 7), 7);
}

TEST_F(SettingsValuesTest, TheLastIntegerSetBeforeSaveIsWrittenOnce) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_int("editor", "tab_width", 8);
    settings.set_int("editor", "tab_width", 4);
    ASSERT_TRUE(settings.save().is_ok());
    EXPECT_TRUE(contains(text(), "tab_width=4"));
    EXPECT_FALSE(contains(text(), "tab_width=8"));
}

TEST_F(SettingsValuesTest, AMissingTextGivesTheFallback) {
    SettingsFile settings(path_);
    settings.load();
    EXPECT_EQ(settings.get_string("editor", "font", "fallback"), std::string("fallback"));
    EXPECT_EQ(settings.get_string("editor", "font", ""), std::string(""));
}

TEST_F(SettingsValuesTest, AnEmptyTextIsNotTheFallback) {
    put("[editor]\nfont=\n");
    SettingsFile settings(path_);
    settings.load();
    EXPECT_EQ(settings.get_string("editor", "font", "fallback"), std::string(""));
}

TEST_F(SettingsValuesTest, ATextIsVisibleBeforeSave) {
    SettingsFile settings(path_);
    settings.load();
    settings.set_string("editor", "font", "DejaVu Sans Mono 11");
    EXPECT_EQ(settings.get_string("editor", "font", ""), std::string("DejaVu Sans Mono 11"));
    EXPECT_FALSE(file_exists());
}

TEST_F(SettingsValuesTest, ATextReadsBackAfterSave) {
    SettingsFile writer(path_);
    writer.load();
    writer.set_string("editor", "font", "DejaVu Sans Mono 11");
    ASSERT_TRUE(writer.save().is_ok());
    EXPECT_TRUE(contains(text(), "font=DejaVu Sans Mono 11"));

    SettingsFile reader(path_);
    reader.load();
    EXPECT_EQ(reader.get_string("editor", "font", ""), std::string("DejaVu Sans Mono 11"));
}

TEST_F(SettingsValuesTest, AnEmptyTextIsWrittenAndReadBack) {
    SettingsFile writer(path_);
    writer.load();
    writer.set_string("editor", "font", "");
    ASSERT_TRUE(writer.save().is_ok());
    EXPECT_TRUE(contains(text(), "font="));

    SettingsFile reader(path_);
    reader.load();
    EXPECT_EQ(reader.get_string("editor", "font", "fallback"), std::string(""));
}

TEST_F(SettingsValuesTest, BlanksLineFeedsAndBackslashesSurviveASave) {
    const std::string tricky = " lead\tTab\nLine\\Back trail ";
    SettingsFile writer(path_);
    writer.load();
    writer.set_string("t", "text", tricky);
    ASSERT_TRUE(writer.save().is_ok());

    SettingsFile reader(path_);
    EXPECT_EQ(reader.load(), SettingsLoad::Loaded);
    EXPECT_EQ(reader.get_string("t", "text", "fallback"), tricky);
}

TEST_F(SettingsValuesTest, ATextWithAnInvalidEscapeGivesTheFallback) {
    put("[t]\ntext=a\\qb\n");
    SettingsFile settings(path_);
    ASSERT_EQ(settings.load(), SettingsLoad::Loaded);
    EXPECT_EQ(settings.get_string("t", "text", "fallback"), std::string("fallback"));
}

TEST_F(SettingsValuesTest, ChangesOfEveryTypeKeepTheUnknownKeys) {
    put("[editor]\nfuture=abc\n[extra]\nkept=yes\n");
    SettingsFile settings(path_);
    settings.load();
    settings.set_int("editor", "tab_width", 8);
    settings.set_string("editor", "font", "Mono 10");
    settings.set_bool("files", "confirm_large_reload", false);
    ASSERT_TRUE(settings.save().is_ok());
    const std::string written = text();
    EXPECT_TRUE(contains(written, "future=abc"));
    EXPECT_TRUE(contains(written, "kept=yes"));
    EXPECT_TRUE(contains(written, "tab_width=8"));
    EXPECT_TRUE(contains(written, "font=Mono 10"));
    EXPECT_TRUE(contains(written, "confirm_large_reload=false"));
}

TEST_F(SettingsValuesTest, TwoWritersOfDifferentTypesDoNotUndoEachOther) {
    SettingsFile first(path_);
    SettingsFile second(path_);
    first.load();
    second.load();
    first.set_int("editor", "tab_width", 8);
    second.set_string("editor", "font", "Mono 10");
    ASSERT_TRUE(first.save().is_ok());
    ASSERT_TRUE(second.save().is_ok());

    SettingsFile reader(path_);
    reader.load();
    EXPECT_EQ(reader.get_int("editor", "tab_width", 4), 8);
    EXPECT_EQ(reader.get_string("editor", "font", ""), std::string("Mono 10"));
}

} // namespace
