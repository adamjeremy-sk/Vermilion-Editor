// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the ctxmon server API parsers against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "ctxmon/core/server_api.h"

using vermilion::ctxmon::model_from_models;
using vermilion::ctxmon::model_from_props;
using vermilion::ctxmon::parse_slots;
using vermilion::ctxmon::SlotInfo;

namespace {

std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The cases of one kind ("slots", "props" or "models") in http.json; parser owns them.
JsonArray* expected_cases(JsonParser* parser, const char* kind) {
    const std::string path = std::string(CTXMON_FIXTURE_DIR) + "/expected/http.json";
    if (!json_parser_load_from_file(parser, path.c_str(), nullptr)) {
        return nullptr;
    }
    JsonNode* root = json_parser_get_root(parser);
    if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
        return nullptr;
    }
    return json_object_get_array_member(json_node_get_object(root), kind);
}

// The response body a case names.
std::string body_of(JsonObject* c) {
    const std::string file = json_object_get_string_member(c, "file");
    return read_bytes(std::string(CTXMON_FIXTURE_DIR) + "/http/" + file);
}

// Each compares a value that may be absent with a JSON member that may be null.
void expect_int(JsonObject* c, const char* name, const std::optional<long long>& value) {
    SCOPED_TRACE(name);
    if (json_object_get_null_member(c, name)) {
        EXPECT_FALSE(value.has_value());
    } else {
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, json_object_get_int_member(c, name));
    }
}

void expect_bool(JsonObject* c, const char* name, const std::optional<bool>& value) {
    SCOPED_TRACE(name);
    if (json_object_get_null_member(c, name)) {
        EXPECT_FALSE(value.has_value());
    } else {
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, json_object_get_boolean_member(c, name) != FALSE);
    }
}

void expect_string(JsonObject* c, const char* name, const std::optional<std::string>& value) {
    SCOPED_TRACE(name);
    if (json_object_get_null_member(c, name)) {
        EXPECT_FALSE(value.has_value());
    } else {
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, std::string(json_object_get_string_member(c, name)));
    }
}

} // namespace

TEST(CtxmonServerApi, SlotsMatchOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = expected_cases(parser, "slots");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 8u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(json_object_get_string_member(c, "file"));
        const SlotInfo info = parse_slots(body_of(c));
        expect_int(c, "n_ctx", info.n_ctx);
        expect_bool(c, "is_processing", info.is_processing);
        expect_int(c, "n_decoded", info.n_decoded);
    }
}

TEST(CtxmonServerApi, PropsMatchOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = expected_cases(parser, "props");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 5u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(json_object_get_string_member(c, "file"));
        expect_string(c, "model", model_from_props(body_of(c)));
    }
}

TEST(CtxmonServerApi, ModelsMatchOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = expected_cases(parser, "models");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 4u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(json_object_get_string_member(c, "file"));
        expect_string(c, "model", model_from_models(body_of(c)));
    }
}

TEST(CtxmonServerApi, SlotThatIsNotAnObjectGivesNothing) {
    const SlotInfo info = parse_slots("[1]");
    EXPECT_FALSE(info.n_ctx.has_value());
    EXPECT_FALSE(info.is_processing.has_value());
    EXPECT_FALSE(info.n_decoded.has_value());
}

TEST(CtxmonServerApi, NonStringNamesCountAsMissing) {
    const auto props = model_from_props(R"({"model_alias":5,"model_path":"/m/x.gguf"})");
    ASSERT_TRUE(props.has_value());
    EXPECT_EQ(*props, std::string("x.gguf"));
    EXPECT_FALSE(model_from_models(R"({"data":[{"id":7}]})").has_value());
}

TEST(CtxmonServerApi, EmptyBodyGivesNothing) {
    EXPECT_FALSE(parse_slots(std::string_view()).n_ctx.has_value());
    EXPECT_FALSE(model_from_props(std::string_view()).has_value());
    EXPECT_FALSE(model_from_models(std::string_view()).has_value());
}
