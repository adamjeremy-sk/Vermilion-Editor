// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for the ctxmon graph geometry against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <optional>
#include <string>
#include <vector>

#include "ctxmon/core/graph.h"

using vermilion::ctxmon::graph_segments;
using vermilion::ctxmon::graph_span;
using vermilion::ctxmon::GraphPoint;
using vermilion::ctxmon::nice_max;
using vermilion::ctxmon::PixelPath;

namespace {

bool is_null(JsonNode* node) {
    return node == nullptr || JSON_NODE_HOLDS_NULL(node);
}

// The cases of one kind ("segments", "nice_max" or "span") in graph.json; parser owns them.
JsonArray* load_cases(JsonParser* parser, const char* kind) {
    const std::string path = std::string(CTXMON_FIXTURE_DIR) + "/expected/graph.json";
    if (!json_parser_load_from_file(parser, path.c_str(), nullptr)) {
        return nullptr;
    }
    JsonNode* root = json_parser_get_root(parser);
    if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
        return nullptr;
    }
    return json_object_get_array_member(json_node_get_object(root), kind);
}

// The history points of a case: a list of [time, value or null].
std::vector<GraphPoint> points_of(JsonObject* c) {
    std::vector<GraphPoint> points;
    JsonArray* list = json_object_get_array_member(c, "points");
    for (guint i = 0; i < json_array_get_length(list); ++i) {
        JsonArray* pair = json_array_get_array_element(list, i);
        GraphPoint p;
        p.time = json_array_get_double_element(pair, 0);
        JsonNode* value = json_array_get_element(pair, 1);
        if (!is_null(value)) {
            p.value = json_node_get_double(value);
        }
        points.push_back(p);
    }
    return points;
}

// The optional argument of a case, or fallback when the case leaves it out (null).
double optional_arg(JsonObject* c, const char* name, double fallback) {
    JsonNode* node = json_object_get_member(c, name);
    return is_null(node) ? fallback : json_node_get_double(node);
}

} // namespace

TEST(CtxmonGraph, SegmentsMatchOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "segments");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 88u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(json_object_get_string_member(c, "series"));
        SCOPED_TRACE(i);
        const std::vector<PixelPath> actual = graph_segments(
            points_of(c), json_object_get_double_member(c, "now"),
            json_object_get_double_member(c, "span"), json_object_get_double_member(c, "ymax"),
            json_object_get_double_member(c, "w"), json_object_get_double_member(c, "h"),
            json_object_get_boolean_member(c, "step") != FALSE);
        JsonArray* expected = json_object_get_array_member(c, "segments");
        ASSERT_EQ(json_array_get_length(expected), actual.size());
        for (guint k = 0; k < json_array_get_length(expected); ++k) {
            JsonArray* path = json_array_get_array_element(expected, k);
            ASSERT_EQ(json_array_get_length(path), actual[k].size());
            for (guint j = 0; j < json_array_get_length(path); ++j) {
                JsonArray* xy = json_array_get_array_element(path, j);
                EXPECT_DOUBLE_EQ(actual[k][j].x, json_array_get_double_element(xy, 0));
                EXPECT_DOUBLE_EQ(actual[k][j].y, json_array_get_double_element(xy, 1));
            }
        }
    }
}

TEST(CtxmonGraph, NiceMaxMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "nice_max");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 44u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(i);
        const double v = json_object_get_double_member(c, "v");
        const double actual = is_null(json_object_get_member(c, "minimum"))
                                  ? nice_max(v)
                                  : nice_max(v, json_object_get_double_member(c, "minimum"));
        EXPECT_DOUBLE_EQ(actual, json_object_get_double_member(c, "result"));
    }
}

TEST(CtxmonGraph, SpanMatchesOracle) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonArray* cases = load_cases(parser, "span");
    ASSERT_NE(cases, nullptr);
    ASSERT_EQ(json_array_get_length(cases), 60u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        SCOPED_TRACE(json_object_get_string_member(c, "series"));
        SCOPED_TRACE(i);
        const double actual = graph_span(points_of(c), json_object_get_double_member(c, "now"),
                                         json_object_get_double_member(c, "window"),
                                         optional_arg(c, "minimum", 60.0));
        EXPECT_DOUBLE_EQ(actual, json_object_get_double_member(c, "span"));
    }
}

TEST(CtxmonGraph, DegenerateFramesGiveNoSegments) {
    const std::vector<GraphPoint> points = {{90.0, 10.0}, {100.0, 20.0}};
    EXPECT_FALSE(graph_segments(points, 100.0, 60.0, 50.0, 300.0, 100.0).empty());
    EXPECT_TRUE(graph_segments(points, 100.0, 0.0, 50.0, 300.0, 100.0).empty());
    EXPECT_TRUE(graph_segments(points, 100.0, 60.0, 0.0, 300.0, 100.0).empty());
    EXPECT_TRUE(graph_segments(points, 100.0, 60.0, 50.0, 0.0, 100.0).empty());
    EXPECT_TRUE(graph_segments(points, 100.0, 60.0, 50.0, 300.0, 0.0).empty());
    EXPECT_TRUE(graph_segments({}, 100.0, 60.0, 50.0, 300.0, 100.0).empty());
}

TEST(CtxmonGraph, StepHoldsTheValueUpToNow) {
    const std::vector<GraphPoint> points = {{90.0, 10.0}};
    const std::vector<PixelPath> plain = graph_segments(points, 100.0, 100.0, 20.0, 200.0, 100.0);
    ASSERT_EQ(plain.size(), 1u);
    ASSERT_EQ(plain[0].size(), 1u);
    EXPECT_DOUBLE_EQ(plain[0][0].x, 180.0);
    EXPECT_DOUBLE_EQ(plain[0][0].y, 50.0);

    const std::vector<PixelPath> held =
        graph_segments(points, 100.0, 100.0, 20.0, 200.0, 100.0, true);
    ASSERT_EQ(held.size(), 1u);
    ASSERT_EQ(held[0].size(), 2u);
    EXPECT_DOUBLE_EQ(held[0][1].x, 200.0);
    EXPECT_DOUBLE_EQ(held[0][1].y, 50.0);
}
