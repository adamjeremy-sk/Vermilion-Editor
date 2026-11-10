// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Server API parsing: slot_info and server_info of the Python original on json-glib.

#include "ctxmon/core/server_api.h"

#include <json-glib/json-glib.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace vermilion::ctxmon {

namespace {

// The root node of body parsed as JSON (owned by parser), or nullptr if it does not parse or
// is too large or too deeply nested to be parsed safely.
JsonNode* parse_root(JsonParser* parser, std::string_view body) {
    if (body.empty()) {
        return nullptr; // also covers a default string_view, whose data() is null
    }
    if (body.size() > kMaxBodyBytes || !json_depth_within(body, kMaxJsonDepth)) {
        return nullptr;
    }
    if (!json_parser_load_from_data(parser, body.data(), static_cast<gssize>(body.size()),
                                    nullptr)) {
        return nullptr;
    }
    return json_parser_get_root(parser);
}

// The object held by node, or nullptr if node is missing or holds something else.
JsonObject* as_object(JsonNode* node) {
    return node != nullptr && JSON_NODE_HOLDS_OBJECT(node) ? json_node_get_object(node) : nullptr;
}

// The first element of an array node, or nullptr if node is not a non-empty array.
JsonNode* first_element(JsonNode* node) {
    if (node == nullptr || !JSON_NODE_HOLDS_ARRAY(node)) {
        return nullptr;
    }
    JsonArray* array = json_node_get_array(node);
    return json_array_get_length(array) > 0 ? json_array_get_element(array, 0) : nullptr;
}

// A member holding a value of the given GType, or nullptr (missing, null, or another type).
JsonNode* value_member(JsonObject* object, const char* name, GType type) {
    JsonNode* node = json_object_get_member(object, name);
    if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) || json_node_get_value_type(node) != type) {
        return nullptr;
    }
    return node;
}

std::optional<long long> int_member(JsonObject* object, const char* name) {
    JsonNode* node = value_member(object, name, G_TYPE_INT64);
    if (node == nullptr) {
        return std::nullopt;
    }
    return static_cast<long long>(json_node_get_int(node));
}

// int_member, but a negative number counts as missing: no count the server reports is below 0.
std::optional<long long> count_member(JsonObject* object, const char* name) {
    const std::optional<long long> value = int_member(object, name);
    if (value && *value < 0) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> bool_member(JsonObject* object, const char* name) {
    JsonNode* node = value_member(object, name, G_TYPE_BOOLEAN);
    if (node == nullptr) {
        return std::nullopt;
    }
    return json_node_get_boolean(node) != FALSE;
}

std::optional<std::string> string_member(JsonObject* object, const char* name) {
    JsonNode* node = value_member(object, name, G_TYPE_STRING);
    if (node == nullptr) {
        return std::nullopt;
    }
    return std::string(json_node_get_string(node));
}

} // namespace

bool json_depth_within(std::string_view text, int max_depth) {
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (const char c : text) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == '[' || c == '{') {
            if (++depth > max_depth) {
                return false;
            }
        } else if ((c == ']' || c == '}') && depth > 0) {
            --depth;
        }
    }
    return true;
}

SlotInfo parse_slots(std::string_view body) {
    SlotInfo info;
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject* slot = as_object(first_element(parse_root(parser, body)));
    if (slot == nullptr) {
        return info;
    }
    info.n_ctx = count_member(slot, "n_ctx");
    info.is_processing = bool_member(slot, "is_processing");
    JsonNode* next = json_object_get_member(slot, "next_token");
    if (next != nullptr && JSON_NODE_HOLDS_ARRAY(next)) {
        next = first_element(next); // newer servers send a list with one entry
    }
    if (JsonObject* token = as_object(next)) {
        info.n_decoded = count_member(token, "n_decoded");
    }
    return info;
}

std::optional<std::string> model_from_props(std::string_view body) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject* props = as_object(parse_root(parser, body));
    if (props == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::string> alias = string_member(props, "model_alias");
    if (alias && !alias->empty()) {
        return alias;
    }
    const std::optional<std::string> path = string_member(props, "model_path");
    if (!path) {
        return std::nullopt;
    }
    // Like Python's os.path.basename: everything after the last '/', so "dir/" gives "".
    const std::size_t slash = path->rfind('/');
    std::string name = slash == std::string::npos ? *path : path->substr(slash + 1);
    if (name.empty()) {
        return std::nullopt;
    }
    return name;
}

std::optional<std::string> model_from_models(std::string_view body) {
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject* list = as_object(parse_root(parser, body));
    if (list == nullptr) {
        return std::nullopt;
    }
    JsonObject* model = as_object(first_element(json_object_get_member(list, "data")));
    if (model == nullptr) {
        return std::nullopt;
    }
    return string_member(model, "id");
}

} // namespace vermilion::ctxmon
