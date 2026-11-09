// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for live_speed and ServerState against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <deque>
#include <optional>
#include <string>

#include "ctxmon/core/server_state.h"

using vermilion::ctxmon::count_kind_name;
using vermilion::ctxmon::CountKind;
using vermilion::ctxmon::LinkState;
using vermilion::ctxmon::live_speed;
using vermilion::ctxmon::Sample;
using vermilion::ctxmon::ServerState;
using vermilion::ctxmon::Snapshot;
using vermilion::ctxmon::SnapshotRequest;
using vermilion::ctxmon::StateView;
using vermilion::ctxmon::TokenKind;

namespace {

JsonNode* member(JsonObject* object, const char* name) {
    return json_object_get_member(object, name);
}

bool is_null(JsonNode* node) {
    return node == nullptr || JSON_NODE_HOLDS_NULL(node);
}

std::optional<double> opt_double(JsonNode* node) {
    return is_null(node) ? std::nullopt : std::optional<double>(json_node_get_double(node));
}

std::optional<long long> opt_int(JsonNode* node) {
    return is_null(node) ? std::nullopt : std::optional<long long>(json_node_get_int(node));
}

std::optional<std::string> opt_string(JsonNode* node) {
    return is_null(node) ? std::nullopt : std::optional<std::string>(json_node_get_string(node));
}

bool as_bool(JsonNode* node) {
    return !is_null(node) && json_node_get_boolean(node) != FALSE;
}

// The cases in file under expected/ (an array); parser owns them.
JsonArray* load_cases(JsonParser* parser, const char* file) {
    const std::string path = std::string(CTXMON_FIXTURE_DIR) + "/expected/" + file;
    if (!json_parser_load_from_file(parser, path.c_str(), nullptr)) {
        return nullptr;
    }
    JsonNode* root = json_parser_get_root(parser);
    return root != nullptr && JSON_NODE_HOLDS_ARRAY(root) ? json_node_get_array(root) : nullptr;
}

// Each compares a value that may be absent with a JSON node that may be null.
void expect_double(const std::optional<double>& actual, JsonNode* expected) {
    if (is_null(expected)) {
        EXPECT_FALSE(actual.has_value());
    } else {
        ASSERT_TRUE(actual.has_value());
        EXPECT_DOUBLE_EQ(*actual, json_node_get_double(expected));
    }
}

void expect_int(const std::optional<long long>& actual, JsonNode* expected) {
    if (is_null(expected)) {
        EXPECT_FALSE(actual.has_value());
    } else {
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(*actual, json_node_get_int(expected));
    }
}

void expect_string(const std::optional<std::string>& actual, JsonNode* expected) {
    if (is_null(expected)) {
        EXPECT_FALSE(actual.has_value());
    } else {
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(*actual, std::string(json_node_get_string(expected)));
    }
}

const char* link_name(LinkState state) {
    switch (state) {
    case LinkState::Up:
        return "up";
    case LinkState::Down:
        return "down";
    case LinkState::Slow:
        return "slow";
    }
    return "";
}

std::optional<TokenKind> token_kind_of(JsonNode* node) {
    const std::optional<std::string> name = opt_string(node);
    if (!name) {
        return std::nullopt;
    }
    return *name == "final" ? TokenKind::Final
                            : (*name == "prompt" ? TokenKind::Prompt : TokenKind::Past);
}

SnapshotRequest request_of(JsonObject* o) {
    SnapshotRequest r;
    r.request.prompt_tokens = opt_int(member(o, "prompt_tokens"));
    r.request.prompt_tps = opt_double(member(o, "prompt_tps"));
    r.request.gen_tokens = json_object_get_int_member(o, "gen_tokens");
    r.request.gen_tps = json_object_get_double_member(o, "gen_tps");
    r.request.ctx = opt_int(member(o, "ctx"));
    r.seed = as_bool(member(o, "seed"));
    return r;
}

Snapshot snapshot_of(JsonObject* o) {
    Snapshot s;
    const std::string state = json_object_get_string_member(o, "state");
    s.state =
        state == "down" ? LinkState::Down : (state == "slow" ? LinkState::Slow : LinkState::Up);
    s.used = opt_int(member(o, "used"));
    s.used_kind = token_kind_of(member(o, "used_kind"));
    s.n_ctx = opt_int(member(o, "n_ctx"));
    s.busy = as_bool(member(o, "busy"));
    s.n_decoded = opt_int(member(o, "n_decoded"));
    s.model = opt_string(member(o, "model"));
    s.log_found = as_bool(member(o, "log_found"));
    s.gen = opt_double(member(o, "gen"));
    s.prompt = opt_double(member(o, "prompt"));
    JsonArray* requests = json_object_get_array_member(o, "requests");
    for (guint i = 0; i < json_array_get_length(requests); ++i) {
        s.requests.push_back(request_of(json_array_get_object_element(requests, i)));
    }
    return s;
}

void expect_view(JsonObject* e, const StateView& v) {
    EXPECT_EQ(std::string(link_name(v.state)),
              std::string(json_object_get_string_member(e, "state")));
    if (v.state == LinkState::Slow) {
        return; // the original returns only the state; the caller keeps the old numbers
    }
    EXPECT_EQ(v.ctx, json_object_get_int_member(e, "ctx"));
    EXPECT_EQ(v.shown, json_object_get_int_member(e, "shown"));
    EXPECT_DOUBLE_EQ(v.frac, json_object_get_double_member(e, "frac"));
    EXPECT_EQ(std::string(count_kind_name(v.kind)),
              std::string(json_object_get_string_member(e, "kind")));
    EXPECT_EQ(v.source, std::string(json_object_get_string_member(e, "source")));
    EXPECT_EQ(v.estimating, json_object_get_boolean_member(e, "estimating") != FALSE);
    expect_double(v.live, member(e, "live"));
    expect_double(v.gen, member(e, "gen"));
    expect_double(v.prompt, member(e, "prompt"));
    expect_double(v.eta, member(e, "eta"));
    EXPECT_EQ(v.busy, json_object_get_boolean_member(e, "busy") != FALSE);
    expect_string(v.model, member(e, "model"));
    EXPECT_EQ(v.log_found, json_object_get_boolean_member(e, "log_found") != FALSE);
}

// The histories, when the step records them (long scenarios record only the last step).
void expect_histories(JsonObject* step, const ServerState& state) {
    if (!json_object_has_member(step, "ctx_hist")) {
        return;
    }
    JsonArray* ctx = json_object_get_array_member(step, "ctx_hist");
    ASSERT_EQ(json_array_get_length(ctx), state.ctx_history().size());
    for (guint i = 0; i < json_array_get_length(ctx); ++i) {
        JsonArray* point = json_array_get_array_element(ctx, i);
        EXPECT_DOUBLE_EQ(state.ctx_history()[i].time, json_array_get_double_element(point, 0));
        EXPECT_EQ(state.ctx_history()[i].tokens, json_array_get_int_element(point, 1));
    }
    JsonArray* speed = json_object_get_array_member(step, "speed_hist");
    ASSERT_EQ(json_array_get_length(speed), state.speed_history().size());
    for (guint i = 0; i < json_array_get_length(speed); ++i) {
        JsonArray* point = json_array_get_array_element(speed, i);
        EXPECT_DOUBLE_EQ(state.speed_history()[i].time, json_array_get_double_element(point, 0));
        expect_double(state.speed_history()[i].tps, json_array_get_element(point, 1));
    }
    JsonArray* requests = json_object_get_array_member(step, "requests");
    ASSERT_EQ(json_array_get_length(requests), state.requests().size());
    for (guint i = 0; i < json_array_get_length(requests); ++i) {
        JsonObject* e = json_array_get_object_element(requests, i);
        const auto& r = state.requests()[i];
        expect_int(r.request.prompt_tokens, member(e, "prompt_tokens"));
        expect_double(r.request.prompt_tps, member(e, "prompt_tps"));
        EXPECT_EQ(r.request.gen_tokens, json_object_get_int_member(e, "gen_tokens"));
        EXPECT_DOUBLE_EQ(r.request.gen_tps, json_object_get_double_member(e, "gen_tps"));
        expect_int(r.request.ctx, member(e, "ctx"));
        expect_double(r.time, member(e, "t"));
    }
}

} // namespace

TEST(CtxmonServerState, LiveSpeedMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "live_speed.json");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 13u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        SCOPED_TRACE(i);
        JsonObject* c = json_array_get_object_element(cases, i);
        std::deque<Sample> samples;
        JsonArray* list = json_object_get_array_member(c, "samples");
        for (guint k = 0; k < json_array_get_length(list); ++k) {
            JsonArray* pair = json_array_get_array_element(list, k);
            samples.push_back(
                {json_array_get_double_element(pair, 0), json_array_get_int_element(pair, 1)});
        }
        const std::optional<double> window = opt_double(member(c, "window"));
        expect_double(window ? live_speed(samples, *window) : live_speed(samples),
                      member(c, "speed"));
    }
}

TEST(CtxmonServerState, StateMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* scenarios = load_cases(parser, "server_state.json");
    ASSERT_NE(scenarios, nullptr);
    ASSERT_EQ(json_array_get_length(scenarios), 12u);
    for (guint i = 0; i < json_array_get_length(scenarios); ++i) {
        JsonObject* scenario = json_array_get_object_element(scenarios, i);
        SCOPED_TRACE(json_object_get_string_member(scenario, "name"));
        ServerState state(json_object_get_int_member(scenario, "default_ctx"),
                          json_object_get_double_member(scenario, "window"));
        JsonArray* steps = json_object_get_array_member(scenario, "steps");
        for (guint k = 0; k < json_array_get_length(steps); ++k) {
            SCOPED_TRACE(k);
            JsonObject* step = json_array_get_object_element(steps, k);
            const Snapshot snapshot = snapshot_of(json_object_get_object_member(step, "snapshot"));
            const StateView view =
                state.update(snapshot, json_object_get_double_member(step, "now"));
            expect_view(json_object_get_object_member(step, "view"), view);
            expect_histories(step, state);
        }
    }
}

TEST(CtxmonServerState, CountKindNames) {
    EXPECT_STREQ(count_kind_name(CountKind::Down), "down");
    EXPECT_STREQ(count_kind_name(CountKind::None), "none");
    EXPECT_STREQ(count_kind_name(CountKind::Exact), "exact");
    EXPECT_STREQ(count_kind_name(CountKind::Processing), "processing");
    EXPECT_STREQ(count_kind_name(CountKind::Estimate), "estimate");
}

TEST(CtxmonServerState, SlowSnapshotChangesNothing) {
    ServerState state(1000);
    Snapshot up;
    up.used = 100;
    up.used_kind = TokenKind::Final;
    state.update(up, 1.0);
    ASSERT_EQ(state.ctx_history().size(), 1u);

    Snapshot slow;
    slow.state = LinkState::Slow;
    slow.requests.resize(1);
    slow.requests[0].request.gen_tps = 30.0;
    const StateView view = state.update(slow, 2.0);
    EXPECT_EQ(view.state, LinkState::Slow);
    EXPECT_EQ(state.ctx_history().size(), 1u);
    EXPECT_TRUE(state.speed_history().empty());
    EXPECT_TRUE(state.requests().empty());
}
