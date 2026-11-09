// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the ctxmon GPU memory readers against the values of the Python original.

#include <glib.h>
#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>

#include "ctxmon/core/vram.h"

using vermilion::ctxmon::fmt_gib;
using vermilion::ctxmon::GpuVendor;
using vermilion::ctxmon::parse_nvidia_smi;
using vermilion::ctxmon::read_vram_sysfs;
using vermilion::ctxmon::vendor_key;
using vermilion::ctxmon::vendor_label;
using vermilion::ctxmon::VramInfo;

namespace {

std::string fixture(const std::string& relative) {
    return std::string(CTXMON_FIXTURE_DIR) + "/" + relative;
}

bool is_dir(const std::string& relative) {
    return g_file_test(fixture(relative).c_str(), G_FILE_TEST_IS_DIR) != FALSE;
}

bool is_null(JsonNode* node) {
    return node == nullptr || JSON_NODE_HOLDS_NULL(node);
}

// The text of a fixture file, byte for byte.
std::string read_bytes(const std::string& relative) {
    std::ifstream in(fixture(relative), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The cases of one kind ("sysfs", "nvidia", "gib" or "vendors") in vram.json; parser owns them.
JsonArray* load_cases(JsonParser* parser, const char* kind) {
    const std::string path = fixture("expected/vram.json");
    if (!json_parser_load_from_file(parser, path.c_str(), nullptr)) {
        return nullptr;
    }
    JsonNode* root = json_parser_get_root(parser);
    if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
        return nullptr;
    }
    return json_object_get_array_member(json_node_get_object(root), kind);
}

// Checks actual against one expected result of vram.json: null (nothing) or an object.
void expect_vram(const std::optional<VramInfo>& actual, JsonNode* expected) {
    if (is_null(expected)) {
        EXPECT_FALSE(actual.has_value());
        return;
    }
    ASSERT_TRUE(actual.has_value());
    JsonObject* object = json_node_get_object(expected);
    EXPECT_EQ(actual->used_bytes, json_object_get_int_member(object, "used"));
    EXPECT_EQ(actual->total_bytes, json_object_get_int_member(object, "total"));
    EXPECT_EQ(actual->name, std::string(json_object_get_string_member(object, "name")));
    EXPECT_STREQ(vendor_key(actual->vendor), json_object_get_string_member(object, "vendor"));
}

} // namespace

TEST(CtxmonVram, SysfsMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "sysfs");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 16u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        const std::string root = json_object_get_string_member(c, "root");
        SCOPED_TRACE(root);
        EXPECT_EQ(is_dir(root), root != "drm/missing");
        expect_vram(read_vram_sysfs(fixture(root)), json_object_get_member(c, "result"));
    }
}

TEST(CtxmonVram, NvidiaMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "nvidia");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 16u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        const std::string file = json_object_get_string_member(c, "file");
        SCOPED_TRACE(file);
        const std::string path = "nvidia/" + file;
        ASSERT_TRUE(g_file_test(fixture(path).c_str(), G_FILE_TEST_IS_REGULAR) != FALSE);
        expect_vram(parse_nvidia_smi(read_bytes(path)), json_object_get_member(c, "result"));
    }
}

TEST(CtxmonVram, GibMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "gib");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 13u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(i);
        EXPECT_EQ(fmt_gib(json_object_get_int_member(c, "bytes")),
                  std::string(json_object_get_string_member(c, "text")));
    }
}

// The cases list the vendors in the order of the enum: amd, nvidia, intel, other.
TEST(CtxmonVram, VendorKeysAndLabelsMatchOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "vendors");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 4u);
    const GpuVendor vendors[] = {GpuVendor::Amd, GpuVendor::Nvidia, GpuVendor::Intel,
                                 GpuVendor::Other};
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(i);
        EXPECT_STREQ(vendor_key(vendors[i]), json_object_get_string_member(c, "key"));
        EXPECT_STREQ(vendor_label(vendors[i]), json_object_get_string_member(c, "label"));
    }
}

// Six cards with the same total: the original takes the one its directory listing yields first,
// here it is the first in name order.
TEST(CtxmonVram, SysfsTieKeepsTheFirstCardByName) {
    ASSERT_TRUE(is_dir("drm_hand/tie"));
    const std::optional<VramInfo> info = read_vram_sysfs(fixture("drm_hand/tie"));
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, std::string("card0"));
    EXPECT_EQ(info->used_bytes, 0);
    EXPECT_EQ(info->total_bytes, 4096);
}

// Python's int() accepts these; real counters never contain them, so the card is skipped.
TEST(CtxmonVram, SysfsRejectsSignsUnderscoresAndOverflow) {
    for (const char* name : {"junk_negative", "junk_plus", "junk_underscore", "junk_overflow"}) {
        SCOPED_TRACE(name);
        const std::string path = std::string("drm_hand/") + name;
        ASSERT_TRUE(is_dir(path));
        EXPECT_FALSE(read_vram_sysfs(fixture(path)).has_value());
    }
}

TEST(CtxmonVram, NvidiaRejectsSignsUnderscoresAndOverflow) {
    const std::optional<VramInfo> plain = parse_nvidia_smi("GPU, 5, 100\n");
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->used_bytes, 5 * 1048576);
    EXPECT_EQ(plain->total_bytes, 100 * 1048576);
    for (const char* output : {"GPU, -5, 100\n", "GPU, +5, 100\n", "GPU, 1_0, 100\n",
                               "GPU, 9223372036854775807, 100\n"}) {
        SCOPED_TRACE(output);
        EXPECT_FALSE(parse_nvidia_smi(output).has_value());
    }
}
