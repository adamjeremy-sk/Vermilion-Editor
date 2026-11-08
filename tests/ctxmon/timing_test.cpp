// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the ctxmon timing-line parser against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "ctxmon/core/timing.h"

using vermilion::ctxmon::FinishedRequest;
using vermilion::ctxmon::last_speeds;
using vermilion::ctxmon::parse_requests;

namespace {

std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Compares a value that may be absent with a JSON member that may be null.
void expect_member(JsonObject* object, const char* name, const std::optional<double>& value) {
    SCOPED_TRACE(name);
    if (json_object_get_null_member(object, name)) {
        EXPECT_FALSE(value.has_value());
    } else {
        ASSERT_TRUE(value.has_value());
        EXPECT_DOUBLE_EQ(*value, json_object_get_double_member(object, name));
    }
}

void expect_request(JsonObject* expected, const FinishedRequest& actual) {
    expect_member(expected, "prompt_tokens", actual.prompt_tokens);
    expect_member(expected, "prompt_tps", actual.prompt_tps);
    expect_member(expected, "gen_tokens", static_cast<double>(actual.gen_tokens));
    expect_member(expected, "gen_tps", actual.gen_tps);
    expect_member(expected, "ctx", actual.ctx);
}

} // namespace

TEST(CtxmonTiming, MatchesOracle) {
    const std::string fixtures = CTXMON_FIXTURE_DIR;
    const std::string path = fixtures + "/expected/timing.json";
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) error = nullptr;
    ASSERT_TRUE(json_parser_load_from_file(parser, path.c_str(), &error))
        << (error != nullptr ? error->message : "no error message");
    JsonNode* root = json_parser_get_root(parser);
    ASSERT_TRUE(JSON_NODE_HOLDS_ARRAY(root));
    JsonArray* cases = json_node_get_array(root);
    ASSERT_EQ(json_array_get_length(cases), 4u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        const std::string file = json_object_get_string_member(c, "file");
        SCOPED_TRACE(file);
        const std::string text = read_bytes(fixtures + "/timing/" + file);
        ASSERT_TRUE(!text.empty() || file == "empty.log");
        const auto speeds = last_speeds(text);
        expect_member(c, "gen_tps", speeds.gen_tps);
        expect_member(c, "prompt_tps", speeds.prompt_tps);
        const std::vector<FinishedRequest> requests = parse_requests(text);
        JsonArray* expected = json_object_get_array_member(c, "requests");
        ASSERT_EQ(requests.size(), json_array_get_length(expected));
        for (guint k = 0; k < json_array_get_length(expected); ++k) {
            SCOPED_TRACE(k);
            expect_request(json_array_get_object_element(expected, k), requests[k]);
        }
    }
}

TEST(CtxmonTiming, MalformedRateIsSkipped) {
    const std::string text = "eval time = 10.0 ms / 5 runs ( 2.00 ms per token, "
                             "1.2.3 tokens per second)\n";
    EXPECT_FALSE(last_speeds(text).gen_tps.has_value());
    EXPECT_TRUE(parse_requests(text).empty());
}

TEST(CtxmonTiming, PromptLineIsNotGeneration) {
    const std::string text = "prompt eval time = 50.0 ms / 10 tokens ( 5.00 ms per token, "
                             "200.00 tokens per second)\n";
    const auto speeds = last_speeds(text);
    EXPECT_FALSE(speeds.gen_tps.has_value());
    ASSERT_TRUE(speeds.prompt_tps.has_value());
    EXPECT_DOUBLE_EQ(*speeds.prompt_tps, 200.0);
}
