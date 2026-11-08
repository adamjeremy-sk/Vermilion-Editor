// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Tests for ctxmon display formatting against the values of the Python original.

#include <gtest/gtest.h>
#include <json-glib/json-glib.h>

#include <string>

#include "ctxmon/core/format.h"

using vermilion::ctxmon::fmt_duration;

TEST(CtxmonFormat, FmtDurationMatchesOracle) {
    const std::string path = std::string(CTXMON_FIXTURE_DIR) + "/expected/fmt_duration.json";
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) error = nullptr;
    ASSERT_TRUE(json_parser_load_from_file(parser, path.c_str(), &error))
        << (error != nullptr ? error->message : "no error message");
    JsonNode* root = json_parser_get_root(parser);
    ASSERT_TRUE(JSON_NODE_HOLDS_ARRAY(root));
    JsonArray* cases = json_node_get_array(root);
    ASSERT_EQ(json_array_get_length(cases), 14u);
    for (guint i = 0; i < json_array_get_length(cases); ++i) {
        JsonObject* c = json_array_get_object_element(cases, i);
        const double seconds = json_object_get_double_member(c, "seconds");
        const std::string text = json_object_get_string_member(c, "text");
        SCOPED_TRACE(seconds);
        EXPECT_EQ(fmt_duration(seconds), text);
    }
}

TEST(CtxmonFormat, FmtDurationRoundsHalfToEven) {
    EXPECT_EQ(fmt_duration(0.5), std::string("0s"));
    EXPECT_EQ(fmt_duration(2.5), std::string("2s"));
    EXPECT_EQ(fmt_duration(89.5), std::string("1m30s"));
}
