// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the ctxmon log token scanner against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <chrono>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "ctxmon/core/log_tokens.h"

using vermilion::ctxmon::kTailBytes;
using vermilion::ctxmon::last_token;
using vermilion::ctxmon::tail_start;
using vermilion::ctxmon::token_matches;
using vermilion::ctxmon::TokenKind;
using vermilion::ctxmon::TokenMatch;

namespace {

std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string kind_name(TokenKind kind) {
    switch (kind) {
    case TokenKind::Final:
        return "final";
    case TokenKind::Prompt:
        return "prompt";
    case TokenKind::Past:
        return "past";
    }
    return "?";
}

} // namespace

TEST(CtxmonLogTokens, MatchesOracle) {
    const std::string fixtures = CTXMON_FIXTURE_DIR;
    const std::string path = fixtures + "/expected/last_tokens.json";
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) error = nullptr;
    ASSERT_TRUE(json_parser_load_from_file(parser, path.c_str(), &error))
        << (error != nullptr ? error->message : "no error message");
    JsonNode* root = json_parser_get_root(parser);
    ASSERT_TRUE(JSON_NODE_HOLDS_ARRAY(root));
    JsonArray* cases = json_node_get_array(root);
    ASSERT_EQ(json_array_get_length(cases), 30u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        const std::string file = json_object_get_string_member(c, "file");
        const auto floor = static_cast<std::size_t>(json_object_get_int_member(c, "floor"));
        SCOPED_TRACE(file + " floor " + std::to_string(floor));
        const std::string bytes = read_bytes(fixtures + "/logs/" + file);
        ASSERT_TRUE(!bytes.empty() || file == "empty.log");
        const auto start = tail_start(bytes.size(), floor);
        const auto reading = last_token(std::string_view(bytes).substr(start));
        if (json_object_get_null_member(c, "tokens")) {
            EXPECT_FALSE(reading.has_value());
        } else {
            ASSERT_TRUE(reading.has_value());
            EXPECT_EQ(reading->tokens, json_object_get_int_member(c, "tokens"));
            const std::string kind = json_object_get_string_member(c, "kind");
            EXPECT_EQ(kind_name(reading->kind), kind);
        }
    }
}

TEST(CtxmonLogTokens, TailStartKeepsOnlyTheTail) {
    EXPECT_EQ(tail_start(kTailBytes + 100, 0), 100u);
    EXPECT_EQ(tail_start(kTailBytes + 100, 500), 500u);
    EXPECT_EQ(tail_start(kTailBytes + 100, kTailBytes + 200), 100u); // floor past the end
    EXPECT_EQ(tail_start(10, 20), 0u);
}

TEST(CtxmonLogTokens, OverflowingNumberIsSkipped) {
    const auto reading = last_token("n_past = 7\nn_past = 99999999999999999999\n");
    ASSERT_TRUE(reading.has_value());
    EXPECT_EQ(reading->tokens, 7);
    EXPECT_EQ(kind_name(reading->kind), std::string("past"));
}

TEST(CtxmonLogTokens, DefaultViewHasNoReading) {
    EXPECT_FALSE(last_token(std::string_view()).has_value());
}

TEST(CtxmonLogTokens, TokenMatchesKeepOrderAndPositions) {
    const auto matches = token_matches("ab n_past = 3\nstop processing: n_tokens = 9\n");
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].pos, 3u);
    EXPECT_EQ(matches[0].reading.tokens, 3);
    EXPECT_EQ(matches[1].pos, 14u);
    EXPECT_EQ(matches[1].reading.tokens, 9);
    EXPECT_EQ(kind_name(matches[1].reading.kind), std::string("final"));
}

// A 1 MB line with no newline and a 1 MB number do no harm: the number is skipped and the
// readings around them are found.
TEST(CtxmonLogTokens, OneMegabyteLinesAreHarmless) {
    const std::string text =
        std::string(1000000, 'x') +
        " stop processing: n_tokens = 42 n_past = " + std::string(1000000, '9') + " n_past = 7";
    const std::vector<TokenMatch> matches = token_matches(text);
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].reading.tokens, 42);
    EXPECT_EQ(matches[0].reading.kind, TokenKind::Final);
    EXPECT_EQ(matches[1].reading.tokens, 7);
    EXPECT_EQ(matches[1].reading.kind, TokenKind::Past);
}

// Callers hand over at most the kTailBytes tail, but a text 32 times larger still scans in one
// pass, well within the (wide) time limit.
TEST(CtxmonLogTokens, LargeInputScansQuickly) {
    const std::string line = "slot update_slots: id  0 | task 7 | n_past = 1234, memory_seq_rm\n";
    std::string text;
    while (text.size() < 32 * kTailBytes) {
        text += line;
    }
    const auto start = std::chrono::steady_clock::now();
    const std::vector<TokenMatch> matches = token_matches(text);
    const std::chrono::duration<double> took = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(matches.size(), text.size() / line.size());
    EXPECT_LT(took.count(), 10.0);
}
