# HANDOFF.md — Orchestrator Continuity Log
Maintained by the Orchestrator per AI_ONBOARDING.md §6. A new instance reads this after AI_ONBOARDING.md and ARCHITECTURE.md.

## 1. Current State  *(overwrite every update)*
| Field | Value |
|---|---|
| Updated | 2026-11-07 ~22:15 (session end, evening handoff: README swept; HANDOFF.md restored after a stale write; M1.3 prompt written, not yet issued) |
| FIRST ACTION on every new chat | **HANDOFF.md and AI_ONBOARDING.md live in the claude.ai Project (read them with the Projects tool, `claude/HANDOFF.md`), not on disk: a fresh chat's working directory is empty, so do not look for them there. Once they are read, and before any work on the repo, get access to the repo folder.** Call `get_device_info` first; if `connectedFolders` already lists `/home/adam/Projects/vermilion-editor`, no request is needed. Otherwise call `device_request_folder_access` for that path once (the grant is recursive). If it is denied or unanswered, do not repeat it: say so, and either ask Adam to attach the folder or fall back to gated Qwen prompts. Then work through `device_bash` (the folder is mounted at `$HOME/mnt/vermilion-editor`; read-only git with `git --no-optional-locks`). Note: `device_bash` runs in a VM on Adam's machine, so tool versions seen there (python 3.10, awk) are not Adam's host versions. Editing rights are unchanged: root orchestration docs and `docs/` only, never code. |
| Project | Vermilion Editor: native Linux text and code editor, C++17, GTK 4, GtkSourceView 5, built-in AI agent, build/debug/terminal, model monitor (ctxmon), GPL-3.0-or-later. Written from scratch. |
| Repo state | Verified 2026-11-07 ~22:10: HEAD `b97b1ac` (docs: handoff after M1.2), `3c80113` is the M1.2 gate commit; in sync with `origin/main`. The HANDOFF.md inside `b97b1ac` is a stale copy (identical to `27443d2`'s) because the Orchestrator's last write delivered an old file; this version restores it. Uncommitted, by the Orchestrator: ` M README.md` (end-of-day sweep) and ` M HANDOFF.md`. Adam commits them as "docs: end-of-day handoff, README sweep" and pushes. |
| Version / HEAD | `b97b1ac` plus the end-of-day docs commit. ctxmon core: `format` (`fmt_duration`), `log_tokens` (`tail_start`, `token_matches`, `last_token`), `timing` (`last_speeds`, `parse_requests`); oracle generators `fmt_duration`, `last_tokens`, `timing`. |
| Tests | 77, all passing (M1.2). 83 expected after M1.3. |
| Active task | **M1, ctxmon logic** (gate M1.0 next). M0 is closed and reviewed (`docs/milestones/M0-review.md`). |
| Gate position | M1.2 committed and pushed. M1.3 prompt written (verbatim below), **not yet issued**. Its "Last verified" line names "docs: handoff after M1.2"; when issuing, change that one line to the end-of-day commit "docs: end-of-day handoff, README sweep". M1.3's json-glib code had no real compile yet (syntax-checked against stand-in declarations with json-glib 1.8 signatures; logic checked against all 17 oracle cases with a fake JSON backend). |
| Awaiting | Adam: commit README.md and HANDOFF.md, push. Next session: "Resume from HANDOFF.md", confirm the readback, then run M1.3. Open question to Adam: does the editor still get its own terminal panel (M4.5)? |
| Last prompt issued | M1.3 (written 2026-11-07, verbatim below; not yet sent to Qwen). |
| Next action | On resume: issue M1.3 with its "Last verified" line changed to the end-of-day docs commit (only that line). Audit it: evidence headers at that commit, mtimes after the edits, files byte-identical to the prompt; if the build fails on a json-glib call it is a prompt defect, fixed by a continuation. `http.json` expectations: slots busy 131072/true/57, empty_tokens 8192/false/null, error null, idle 131072/false/null, invalid null, none null, old 32768/true/12, two 4096/false/null; props alias "qwen3.8-27b", invalid null, null null, path "Qwen3.8-27B-Q4_K_M.gguf", trailing_slash null; models blank_id "", empty null, invalid null, one "qwen3.8-27b"; other expected JSON unmodified; 83 tests. Then M1.4 (`live_speed`, `ServerState`, warnings; may split), M1.5 (graph geometry), M1.6 (VRAM readers). Before any UI gate, talk with Adam about his UI specifications. README.md only at the end-of-day handoff. |
| Agent / runtime | Qwen3.8-27B (local llama.cpp, `qwen-server`, port 8080) driven by OpenCode 1.19.5 over ACP inside CLion's Air plugin. Qwen is the only coding agent. Gemma permanently banned. Fresh agent session per prompt; commit only at task end (gate end). Adam builds and tests. Adam codes only at home on this computer with the chat linked, so the Orchestrator can always read `evidence/`. |

**Open notes (no carried debt, Adam 2026-11-06; each is scheduled in a gate)**: none open in code. Accepted design, not debt: `value_or` needs copyable `T`; `key=` query parameters are deliberately not masked; rule A's authorization branch stops only at quotes, comma, CR and LF (values like `Basic xyz` contain spaces). Standing process facts: GLib 2.80 `g_object_add_weak_pointer` does not set the pointer on add (tests preset it); reports show raw failing build output first; `git diff` shows nothing for untracked files, so prompts ask for new files in full; prompts must keep every C++ line they dictate within 100 columns; Qwen replaced raw output with summaries twice (M0.5d1, M0.5d2), answered from M0.5c1 on by AGENTS.md rule 18 (evidence files the Orchestrator reads itself; Adam trusts Qwen not to fabricate them, the Orchestrator still checks headers and mtimes for staleness); clang-format is 18.1.3 on Adam's machine and not installed in the Orchestrator's VM; do not run dependent Orchestrator tool calls in parallel; **prompts and AGENTS.md never mention what the Orchestrator checks or reads (Adam, 2026-11-06)**; check clang-format option names against version 18 before dictating them. Prompt-authoring practices (format step, no dictated whitespace, header lines for new files, FILE evidence, silence about the Orchestrator's checks, tool-version check) are in AI_ONBOARDING.md section 2 item 8. Topics already collected for the M0 review: raw-output summarising and rule 18; format-style choice and rule 19 (the Orchestrator's wrong clang-format key); prompt bugs found by Qwen (`EXPECT_EQ` on `const char*`, over-long dictated lines); GLib weak-pointer quirk; gate sizes. Product notes: dogfooding markers in `docs/roadmap.md` (first at M4.3); CTXMon is a tab plus standalone `vermilion-ctxmon` (names proposed); GTK 4 has no tray icon.
**Open decisions (Adam's call)**: none block M0. Names of config files and JSON schemas are fixed when first written (ARCHITECTURE.md section 10); gdb DAP reliability is checked at M6.2; packaging at M8.

## Last prompt (verbatim)
FIRST: read AGENTS.md in the project root before doing anything else. Its rules are binding for this entire task and override anything below if they conflict. If you cannot read it, say so and STOP.
Last verified: New task; tree clean at the commit "docs: handoff after M1.2" (HEAD).

TASK: Gate M1.3, ctxmon server API parsing. One step.

CONTEXT (BINDING)
- ctxmon logic lives in `src/ctxmon/core` (library `vermilion_ctxmon_core`, namespace `vermilion::ctxmon`, no GTK headers; GLib and json-glib are allowed). Tests live in `tests/ctxmon` (executable `vermilion_ctxmon_tests`). Expected values come from the frozen Python original `reference/ctxmon/ctxmon_gtk.py` (read-only) through `tools/ctxmon_oracle.py`.
- This gate ports the parsing half of `slot_info` and `server_info` (ctxmon_gtk.py lines 232 to 272) as pure functions on response bodies, in a new module `server_api`: `parse_slots` (GET /slots), `model_from_props` (GET /props) and `model_from_models` (GET /v1/models). The HTTP requests themselves come later (M2.8). The oracle runs the Python functions with their `urlopen` answered from the fixture files.
- Test count: 77 now, 83 after this gate.

STRICT RAILS
- Create exactly these files: `src/ctxmon/core/server_api.h`, `src/ctxmon/core/server_api.cpp`, `tests/ctxmon/server_api_test.cpp`, the 17 fixture files of step 1 in `tests/ctxmon/fixtures/http/`, and `tests/ctxmon/fixtures/expected/http.json` (written by the oracle, never by hand).
- Modify exactly these files: `tools/ctxmon_oracle.py`, `src/ctxmon/core/CMakeLists.txt`, `tests/ctxmon/CMakeLists.txt`.
- Re-running the oracle rewrites the other three expected JSON files with identical content: `git status` must not list them as modified.
- No other change. No new dependency (json-glib is already found by the top-level `CMakeLists.txt`).

STEP 1
1. Create the fixture files with exactly these commands, run from the project root (the bytes matter, so do not create them with a file editor):
```bash
mkdir -p tests/ctxmon/fixtures/http && cd tests/ctxmon/fixtures/http
printf '[{"id":0,"n_ctx":131072,"is_processing":true,"next_token":[{"has_next_token":true,"n_remain":-1,"n_decoded":57}]}]\n' > slots_busy.json
printf '[{"id":0,"n_ctx":32768,"is_processing":true,"next_token":{"n_decoded":12}}]\n' > slots_old.json
printf '[{"id":0,"n_ctx":131072,"is_processing":false}]\n' > slots_idle.json
printf '[{"id":0,"n_ctx":8192,"is_processing":false,"next_token":[]}]\n' > slots_empty_tokens.json
printf '[{"id":0,"n_ctx":4096,"is_processing":false},{"id":1,"n_ctx":2048,"is_processing":true}]\n' > slots_two.json
printf '[]\n' > slots_none.json
printf '{"error":{"code":501,"message":"This server does not support slots endpoint."}}\n' > slots_error.json
printf 'not json\n' > slots_invalid.json
printf '{"model_alias":"qwen3.8-27b","model_path":"/models/Qwen3.8-27B-Q4_K_M.gguf","n_ctx":131072}\n' > props_alias.json
printf '{"model_path":"/models/Qwen3.8-27B-Q4_K_M.gguf"}\n' > props_path.json
printf '{"model_alias":"","model_path":"/models/dir/"}\n' > props_trailing_slash.json
printf '{"model_alias":null,"model_path":null}\n' > props_null.json
printf '<html>not found</html>\n' > props_invalid.json
printf '{"object":"list","data":[{"id":"qwen3.8-27b","object":"model"}]}\n' > models_one.json
printf '{"object":"list","data":[]}\n' > models_empty.json
printf '{"object":"list","data":[{"id":""}]}\n' > models_blank_id.json
printf 'Internal Server Error\n' > models_invalid.json
cd - > /dev/null
```
2. Replace the whole content of `tools/ctxmon_oracle.py` with exactly:
```python
#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
# Writes the expected values for the ctxmon tests by running the frozen Python original.
"""Usage: python3 tools/ctxmon_oracle.py  (rewrites tests/ctxmon/fixtures/expected/)"""
import importlib.util
import io
import json
import pathlib
import sys
import urllib.error
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
ORIGINAL = ROOT / "reference" / "ctxmon" / "ctxmon_gtk.py"
EXPECTED = ROOT / "tests" / "ctxmon" / "fixtures" / "expected"
LOGS = ROOT / "tests" / "ctxmon" / "fixtures" / "logs"
TIMING = ROOT / "tests" / "ctxmon" / "fixtures" / "timing"
HTTP = ROOT / "tests" / "ctxmon" / "fixtures" / "http"
URL = "http://127.0.0.1:8080"


def load_original():
    sys.dont_write_bytecode = True  # keep reference/ free of __pycache__
    spec = importlib.util.spec_from_file_location("ctxmon_gtk", ORIGINAL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def fmt_duration_cases(m):
    inputs = [0, 0.4, 0.5, 1.5, 2.5, 59.5, 89.4, 89.5, 90, 90.5, 125, 3599.5, 3600, 7322]
    return [{"seconds": s, "text": m.fmt_duration(s)} for s in inputs]


def last_tokens_cases(m):
    cases = []
    for path in sorted(LOGS.glob("*.log")):
        size = path.stat().st_size
        for floor in (0, 1, size // 2, size, size + 100):
            tokens, kind, _found = m.last_tokens(str(path), floor)
            cases.append({"file": path.name, "floor": floor, "tokens": tokens, "kind": kind})
    return cases


def timing_cases(m):
    cases = []
    for path in sorted(TIMING.glob("*.log")):
        text = path.read_text(encoding="utf-8", errors="replace")
        gen_tps, prompt_tps = m.last_speeds(text)
        cases.append({"file": path.name, "gen_tps": gen_tps, "prompt_tps": prompt_tps,
                      "requests": m.parse_requests(text)})
    return cases


def answering(m, routes, call):
    """Runs call() while the original's urlopen answers from routes (URL path -> file)."""

    def fake_urlopen(url, timeout=None):
        path = urllib.parse.urlparse(url).path
        if path not in routes:
            raise urllib.error.URLError("connection refused")
        return io.BytesIO(routes[path].read_bytes())

    saved = m.urlopen
    m.urlopen = fake_urlopen
    try:
        return call()
    finally:
        m.urlopen = saved


def http_cases(m):
    slots, props, models = [], [], []
    for path in sorted(HTTP.glob("slots_*.json")):
        n_ctx, busy, n_decoded, _state = answering(m, {"/slots": path}, lambda: m.slot_info(URL))
        slots.append({"file": path.name, "n_ctx": n_ctx, "is_processing": busy,
                      "n_decoded": n_decoded})
    for path in sorted(HTTP.glob("props_*.json")):
        model = answering(m, {"/props": path}, lambda: m.server_info(URL))
        props.append({"file": path.name, "model": model})
    for path in sorted(HTTP.glob("models_*.json")):
        model = answering(m, {"/v1/models": path}, lambda: m.server_info(URL))
        models.append({"file": path.name, "model": model})
    return {"slots": slots, "props": props, "models": models}


GENERATORS = {
    "fmt_duration": fmt_duration_cases,
    "last_tokens": last_tokens_cases,
    "http": http_cases,
    "timing": timing_cases,
}


def main():
    m = load_original()
    EXPECTED.mkdir(parents=True, exist_ok=True)
    for name, generate in sorted(GENERATORS.items()):
        path = EXPECTED / (name + ".json")
        path.write_text(json.dumps(generate(m), indent=2) + "\n", encoding="utf-8")
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
```
3. Run: `python3 tools/ctxmon_oracle.py; echo "exit=$?"`
4. Create `src/ctxmon/core/server_api.h` with exactly:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Parses the llama.cpp server's /slots, /props and /v1/models response bodies.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vermilion::ctxmon {

// What the first slot of GET /slots reports. Every field is absent when the body is not JSON,
// not a non-empty array, or its first element is not an object (the Python original treats
// those as "the server answered, but without slot data").
struct SlotInfo {
    std::optional<long long> n_ctx;
    std::optional<bool> is_processing;
    std::optional<long long> n_decoded; // from next_token, which is an object or a list of them
};

SlotInfo parse_slots(std::string_view body);

// The model name in a GET /props body: model_alias if it is a non-empty string, otherwise the
// file name of model_path. Absent when neither gives a non-empty name or the body is not a
// JSON object. A member that is not a string counts as missing.
std::optional<std::string> model_from_props(std::string_view body);

// The id of the first model in a GET /v1/models body (may be empty, as in the Python original),
// or absent when there is none, it is not a string, or the body is not JSON.
std::optional<std::string> model_from_models(std::string_view body);

} // namespace vermilion::ctxmon
```
5. Create `src/ctxmon/core/server_api.cpp` with exactly:
```cpp
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

// The root node of body parsed as JSON (owned by parser), or nullptr if it does not parse.
JsonNode* parse_root(JsonParser* parser, std::string_view body) {
    if (body.empty()) {
        return nullptr; // also covers a default string_view, whose data() is null
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

SlotInfo parse_slots(std::string_view body) {
    SlotInfo info;
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject* slot = as_object(first_element(parse_root(parser, body)));
    if (slot == nullptr) {
        return info;
    }
    info.n_ctx = int_member(slot, "n_ctx");
    info.is_processing = bool_member(slot, "is_processing");
    JsonNode* next = json_object_get_member(slot, "next_token");
    if (next != nullptr && JSON_NODE_HOLDS_ARRAY(next)) {
        next = first_element(next); // newer servers send a list with one entry
    }
    if (JsonObject* token = as_object(next)) {
        info.n_decoded = int_member(token, "n_decoded");
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
```
6. In `src/ctxmon/core/CMakeLists.txt`:
   - change the first line to `add_library(vermilion_ctxmon_core STATIC format.cpp log_tokens.cpp timing.cpp server_api.cpp)`;
   - change the last line to `target_link_libraries(vermilion_ctxmon_core PRIVATE vermilion_warnings vermilion_noexc PkgConfig::GLIB2 PkgConfig::JSONGLIB)`.
7. In `tests/ctxmon/CMakeLists.txt`, change the first line to `add_executable(vermilion_ctxmon_tests format_test.cpp log_tokens_test.cpp timing_test.cpp server_api_test.cpp)`.
8. Create `tests/ctxmon/server_api_test.cpp` with exactly:
```cpp
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
```
9. Run: `clang-format -i src/ctxmon/core/server_api.h src/ctxmon/core/server_api.cpp tests/ctxmon/server_api_test.cpp; echo "exit=$?"`
10. Configure, build and test. Each command writes a FILE in the exact form of AGENTS.md rule 18, gate `M1.3`:
    - `evidence/M1.3/configure.txt` from `cmake -S . -B build`
    - `evidence/M1.3/build.txt` from `cmake --build build`
    - `evidence/M1.3/ctest.txt` from `ctest --test-dir build --output-on-failure`

EVIDENCE (AGENTS.md rules 12 and 18; FILE items are written by the command itself, then only their `tail -n 3` is pasted)
E1. Raw: `git status --short --untracked-files=all`
E2. FILE `evidence/M1.3/diff.txt` from `git diff`
E3. FILE `evidence/M1.3/new_files.txt` from `cat -n src/ctxmon/core/server_api.h src/ctxmon/core/server_api.cpp tests/ctxmon/server_api_test.cpp`
E4. FILE `evidence/M1.3/http_json.txt` from `cat tests/ctxmon/fixtures/expected/http.json`
E5. Raw: `wc -c tests/ctxmon/fixtures/http/*.json`
E6. Raw: the output of steps 3 and 9.
E7. Raw: `wc -l` of every created or modified file except the fixture files.
E8. Raw: `grep -rnE "gtk/|gdk|pango|gtksourceview|vte|std::regex" src/ctxmon/core; echo "exit=$?"`
E9. For each of the six evidence files: its path, then the raw output of `tail -n 3` on it.
E10. Raw: `grep -E "tests passed|tests failed" evidence/M1.3/ctest.txt`
E11. Unexpected files: any file the commands created that the rails do not name, or "none" (AGENTS.md rule 8).

End per AGENTS.md rule 15 with "STOPPING AND WAITING FOR VERIFICATION".

## 2. Decision Log  *(append only, one line each, newest last)*
- 2026-11-05 — (Adam) **Reset.** The project is rebuilt from scratch. The earlier code base is abandoned together with every decision tied to it (MIT licence, custom editing widget, its syntax format, regex engine and feature set, and its removal roadmap).
- 2026-11-05 — (Adam) **Scope trimmed** to: a directory sidebar on the right-hand side (specification kept in `docs/requirements/directory-sidebar.md`), an integrated agent (OpenAI standard, Anthropic and Gemini supported), the ctxmon model monitor ported from `~/Projects/llm-suite-test/ctxmon_gtk.py`, building code in the application and debugging it. Added: a terminal (VTE, also in the monitor), a project file, slash commands with a gate framework, MCP, cost display. Dropped from the first scope: git integration, multi-cursor, plugin hook, theme system, SSH, session restore, indexed project search.
- 2026-11-05 — (Adam) **Language C++17** (after weighing C): RAII and `std::string` for the logic-heavy modules; GTK 4 and GLib through their plain C APIs. **No C++ exceptions in project code** (GTK callbacks are C). **No wrapper rule**: nothing that exists only to wrap another API; libraries that provide a widget or service (GtkSourceView, VTE, libsoup, json-glib, libsecret) are fine.
- 2026-11-05 — (Adam) **Licence GPL-3.0-or-later**, copyright "Adam Hainstock (@adamjeremy-sk)", SPDX header on every source file. Supersedes MIT. The licence text is taken from the GPL-3.0 text in `llm-suite-test` (Adam's own repository, which holds ctxmon).
- 2026-11-05 — (Adam) **Editor widget: GtkSourceView 5.** Target system Ubuntu 24.04.5 (gdb 15.1; GTK 4.14 expected). Development packages to install: `libgtk-4-dev libgtksourceview-5-dev libjson-glib-dev libsoup-3.0-dev libsecret-1-dev libvte-2.91-gtk4-dev libgtest-dev cmake pkg-config clang-format clang-tidy build-essential`.
- 2026-11-05 — (Adam) **Agent**: own agent loop with provider adapters (OpenAI-compatible first, which also reaches Gemini's compatible endpoint; native Anthropic; native Gemini only if needed); streaming; direct model access with user keys in the keyring; tools run_command and the seven others. **Permission presets** Restricted (default), Balanced (Adam's preference, like the coding session), Unrestricted, and Custom JSON. Unrestricted needs a confirmation dialog and a persistent red badge; in every preset the agent cannot change its own preset and cannot read API keys. Setup dialog on first use of the Agent tab. `/clear` archives the chat; `/history`, `/copy`; Copy buttons everywhere; slash commands as Markdown templates, with the gate framework (`/gate`, `/audit`) as built-in ones. Cost display with user-defined input and output prices (including for a local model). Protocols: native loop and MCP (server and client over stdio); ACP deferred. Context, streaming, keys, review, undo, privacy and limits as in ARCHITECTURE.md section 7.6.
- 2026-11-05 — (Adam) **Review and external changes**: side-by-side diff (option B) with folding and per-hunk Accept/Revert; external changes reload a clean buffer automatically and show a banner for a dirty one.
- 2026-11-05 — (Adam) **Debugger**: Debug Adapter Protocol, gdb's built-in DAP first (gdb 15.1), GDB/MI as the fallback if the M6.2 spike shows DAP unreliable. **Build**: per-folder configuration in `.vermilion/project.json` with presets for CMake, Make, Ninja, Cargo, Gradle and Bazel, problem matchers for gcc, clang, rustc, javac.
- 2026-11-05 — (Adam) **Project file** (`PROJECT.md` plus `.vermilion/project.json`; names proposed) tracks the project layout and decisions as the user builds; created by New Project and by "Adopt as project".
- 2026-11-05 — (Adam) **Order and versions**: v0.1 editor, sidebar, build/Output/Problems, terminal; v0.2 agent core and diff; v0.3 debugger; v0.4 monitor panel, cost, Anthropic/Gemini, project file, gate framework, MCP. ctxmon logic (M1) is built early as the first job for Qwen, with tests derived from the Python original. Application ID `io.github.adamjeremy_sk.Vermilion`.
- 2026-11-05 — **Reset kept files** (Orchestrator proposal, Adam confirmed "kept = icons plus process docs"): `res/vedt/`, `LICENSE`, `opencode.json`, `AGENTS.md`, `AI_ONBOARDING.md`, `ARCHITECTURE.md`, `HANDOFF.md`, `README.md`, `docs/roadmap.md`, `docs/requirements/`, `.gitignore` (plus an archive note, removed on 2026-11-07). Everything else is deleted by M0.1. The claude.ai Roadmap Doc is obsolete; `docs/roadmap.md` is the roadmap.

- 2026-11-05 — M0.1 (reset, commit `67e7e70 (was 39d9016 before the rewrite)`) done by Adam: 14 tracked files remain, `upstream` removed. M0.2 (CMake skeleton and empty window) executed by Qwen and audited PASS WITH NOTES. Orchestrator re-check with fresh commands: only the expected files, SPDX header present in all 7 sources, no GTK names in `src/core` or `src/agent`, GTK 4 APIs only, ignore rules cover `build`, `cmake-build-*`, `.idea`, `skills`. Qwen found that the two tests failed because `EXPECT_EQ` compared `const char*` pointers (the Orchestrator's prompt said "equals" without naming the comparison); fixed by comparing `std::string`. Notes: (1) `vermilion_core` links `vermilion_warnings` and `vermilion_noexc` as PUBLIC, so `-fno-exceptions` also reaches `vermilion_agent`, the UI and the tests (compile commands show it on all 5 files); the spec said the tests must not get it. Fixed in M0.3. (2) The window has not been opened by the Orchestrator or by Qwen (no display); Adam checks it. Qwen found GTK 4.14.5, GtkSourceView 5.12.0, json-glib 1.8.0, libsoup 3.4.4, libsecret 0.21.4, VTE 0.76.0, GoogleTest 1.14.0.
- 2026-11-05 — M0.3 (linkage fix, `gtkx` `ObjectPtr` and string helpers, 12 tests) executed by Qwen, audited PASS WITH NOTES (see section 1 carry-forward notes). Orchestrator fresh check: `-fno-exceptions` present on the 4 non-test sources and absent on the 4 test sources, `-Wall` and `-Werror` on all 8, SPDX present in all .cpp/.h, no GTK names in `src/core` or `src/agent`, no GTK 3 calls.
- 2026-11-05 — (Adam) **History rewrite**: drop all history before the M0.1 reset commit (`39d9016`), which becomes the root; replay later commits; one force-push to `origin main` after the M0.3 commit; backup tag kept locally. Reason: the earlier code base lives in older history. Old objects may stay reachable on GitHub by hash until GitHub purges them. Done 2026-11-05: new root `67e7e70`, force-pushed as `13ee5bc`; trees identical to the backup (`git diff --stat` empty).
- 2026-11-05 — M0.4 (`SignalConnection`, `connect<Sig>`, 11 tests) executed by Qwen, audited PASS WITH NOTES (notes in section 1). Fresh check: `-fno-exceptions` on the 5 non-test sources and off the 5 test sources, `-Werror` on all 10, SPDX in all .cpp/.h, no GTK names in core or agent, the only `g_signal_connect` hits are `signal.h` (`_data`) and `main.cpp`. `std::function` compiles under `-fno-exceptions`.
- 2026-11-05 — (Adam) **File size and responsibility** (AGENTS.md rule 16, ARCHITECTURE.md section 5): one responsibility per file; review above 300 lines, split above 500 (tests and data 500 and 800), functions over 50 lines flagged; UI one widget or panel class per file pair; `wc -l` in every report; M0.5c enforces it with a script. M0.5 split into M0.5a, b, c.
- 2026-11-05 — (Adam) **Dogfooding**: the first point at which Vermilion builds itself is M4.3 (Output pane); a second marker at M4.6 (primary editor, CLion kept only as the Qwen host) and a third at M5.5 (built-in agent trial, Orchestrator proposal) are recorded in `docs/roadmap.md`. The earlier proposal at M2.2 was dropped.
- 2026-11-05 — M0.5a (`Error`, `Unit`, `Result<T>`, `Status`, 9 tests, 34 in total) executed by Qwen, audited PASS WITH NOTES (notes in section 1).
- 2026-11-05 — (Orchestrator, within the gate-size rule) **M0.5b split**: b1 `redact_secrets` (pure text, no GLib) and b2 the log layer. Wrapper-rule decision for logging: a thin `vermilion::log` layer is allowed because it adds behaviour (redaction of secrets before `g_log`); it is not a pure wrapper.
- 2026-11-05 — (Adam) **ctxmon original copied into the repo**: `reference/ctxmon/ctxmon_gtk.py` (1,587 lines, sha256 `895a34f7...d407`) and its logo `reference/ctxmon/ctxmon.svg` (sha256 `5b6743ec...28f5`), copied (not moved; `llm-suite-test` keeps its files) from `~/Projects/llm-suite-test`. Read-only reference: it is the oracle for the M1 tests and is not built. The source was last modified 2026-11-05 19:40, so if Adam edits the original again before M1, re-copy and record the new hash. The logo is for the M7.1 monitor panel / window icon.
- 2026-11-05 — M0.5b1 (`redact_secrets`, 15 tests, 49 in total) executed by Qwen, audited PASS WITH NOTES (notes in section 1).
- 2026-11-05 — (Adam) **CTXMon has two forms**: a third tab in the right-hand stack (**Files | Agent | CTXMon**) and a standalone program (`vermilion-ctxmon`, name proposed) built from the same source and widget set. README, ARCHITECTURE.md section 3 and 7.7 and the roadmap (M7.1 tab, new M7.1b standalone) updated. Open: when to build the standalone (it needs only M1 and a window, so it could come before v0.1); Adam decides.
- 2026-11-06 — M0.5b2 (`vermilion::logging` over `g_log`, domain `vermilion`, redaction before every message, `error()` at CRITICAL, 5 tests, 54 in total) executed by Qwen, audited PASS WITH NOTES (notes in section 1). Orchestrator (within the gate-size rule): M0.5c is a bash script `tools/check_sources.sh` (bash, find, sort, awk only; no Python) registered as ctest `SourceCheck.Repo`; Adam-approved size exceptions (AGENTS.md rule 16) are not yet supported by the script and are added when the first one is approved.
- 2026-11-06 — M0.5b2 fix 1 (`handler_id_` to `guint`) done in the same Qwen session; README now documents `G_MESSAGES_DEBUG` and lost a stale M0.2 remark.
- 2026-11-06 — (Adam) **No carried debt, fix loops.** Every audit note is fixed before the gate commit; small fixes go to Qwen as same-session CONTINUATION prompts (no wipe); if Qwen is low on context Adam says so and the fix rolls into the next full prompt. Orchestrator proposals accepted by Adam: write it into AGENTS.md (rule 17) and AI_ONBOARDING.md section 1 and 2; continuation prompt format; one gate commit with fixes folded in; debt definition (defects, includes, types, purpose comments, untested paths of existing code, stale docs; remarks that depend on unwritten code go to the gate that writes them); clear the older backlog now as M0.5d1 and M0.5d2 before M0.5c. Rule 16 detail: the purpose comment is the line directly after the copyright line, and M0.5c checks it.
- 2026-11-06 — M0.5d1 (gtkx `detail` namespace, `operator bool` comment, 3 `ObjectPtr` assignment tests, disconnect-inside-handler test, `main.cpp` on `gtkx::connect`, purpose comments; 58 tests) executed by Qwen; first report FAIL (summary table instead of raw output; four lines over 100 columns dictated by the prompt), fix 1 in the same session, then PASS. Adam confirmed the window opens.
- 2026-11-06 — (Adam) **clang-format check**: a ctest `Format.Check` runs `clang-format --dry-run --Werror` on every `.cpp` and `.h` in `src` and `tests`; it comes with M0.5c, after a one-time format pass M0.5d3.
- 2026-11-06 — M0.5d2 (purpose comments in core, agent and their tests; `<utility>` in `result.h`; 2 death tests; `\v`/`\f` stop in rule A; 5 redact tests; 65 tests) executed by Qwen; report summarised raw output (ctest tail only, diff "see above"); fix 1 (evidence only) gave the full raw ctest, then PASS. Orchestrator verified the diff fresh.
- 2026-11-06 — (Adam) **Milestone review**: at the end of every milestone (after its last gate and docs sweep are pushed) the Orchestrator and Adam discuss what was accomplished, what was learned and what the next milestone is, before any prompt of the next milestone. Conclusions in `docs/milestones/M<n>-review.md`. Written into AI_ONBOARDING.md section 1 and section 2 item 7 and the roadmap header.
- 2026-11-06 — M0.5d3 (one-time `clang-format -i`, clang-format 18.1.3) executed by Qwen, audited PASS: 20 files, formatting only plus using-declaration sorting; 65 tests.
- 2026-11-06 — (Adam) **Evidence files**: long command output goes to `evidence/<gate>/<name>.txt` (git-ignored), written by the command itself; Qwen reports the tail and the path; the Orchestrator reads the files directly. Adam trusts Qwen not to fabricate them (unlike Gemma). AGENTS.md rule 18; `.gitignore` line added in M0.5c1.
- 2026-11-06 — (Orchestrator, within the gate-size rule) **M0.5c split**: c1 `check_sources.sh` + `SourceCheck.Repo` + `evidence/` ignore; c2 `check_format.sh` + `Format.Check` (skip code 77 when clang-format is missing).
- 2026-11-06 — (Adam, on the Orchestrator's recommendation) **Format style**: `.clang-format` adds `AccessModifierOffset: -4` and `AlwaysBreakTemplateDeclarations: Yes` so the checked style matches how Qwen writes; M0.5d3 fix 2. AGENTS.md rule 19: Qwen runs `clang-format -i` on its own changed files before every report, so `Format.Check` does not fail on style. Prompts and AGENTS.md do not mention the Orchestrator's own checks.
- 2026-11-06 — (Adam) **Prompt-authoring practices** recorded as AI_ONBOARDING.md section 2 item 8: explicit `clang-format -i` step on the changed files before every build, no dictated whitespace, exact header lines for new files, FILE evidence, no mention of the Orchestrator's checks, tool options checked against Adam's versions.
- 2026-11-07 — M0.5c1 (`tools/check_sources.sh`, ctest `SourceCheck.Repo`, `evidence/` ignored; 66 tests) executed by Qwen, audited PASS with no notes. First gate under AGENTS.md rule 18: all eight evidence files had correct headers (HEAD `1011dac`) and mtimes after the edits. Orchestrator spot checks: blank line after the copyright line gives MISSING-PURPOSE, empty input gives MISSING-HEADER, running from another directory still checks the 25 repo files. Shell scripts have lines over 100 columns (one is the awk program the prompt dictated); the 100-column rule covers C++ only, so this is not debt.
- 2026-11-07 — M0.5c2 (`tools/check_format.sh`, ctest `Format.Check` with skip code 77; 67 tests, none skipped) executed by Qwen, audited PASS with no notes: eight evidence headers at HEAD `4cb9ed7` with mtimes after the edits; the dirty run shows real `-Wclang-format-violations` errors; `check_sources.sh` sees 26 files. Last code gate of M0. Qwen still writes "(empty)" for empty output in its chat report; harmless now that long output is read from evidence files.
- 2026-11-07 — (Adam) The Orchestrator edits README.md itself for the M0 documentation sweep instead of a Qwen gate. Sweep done by the Orchestrator: README.md (status banner, Status table, Building checks paragraph and `evidence/`, Repository layout `gtkx/` and `tools/`), ARCHITECTURE.md (section 4 heading and section 5 Style no longer say "F0", `tools/` checks paragraph, stale open item removed), docs/roadmap.md (M0 marked done, 67 tests). Whether later sweeps follow this pattern is an open point for the M0 review.

- 2026-11-07 — (Adam) **M0 milestone review held**; record in `docs/milestones/M0-review.md`. M1 is core work only, the GUI comes later; revised gates M1.0 to M1.6 in `docs/roadmap.md`.
- 2026-11-07 — (Adam) **ctxmon is developed in this repository** and Vermilion's port is its source of truth. Layout `src/ctxmon/core` (no GTK), `src/ctxmon/ui`, `tests/ctxmon`; core and UI never mix. The Python original in `reference/ctxmon/` is frozen as the test oracle (supersedes the 2026-11-05 "re-copy if edited" note); whether it is removed is decided at the M2 review.
- 2026-11-07 — (Adam) **Regular expressions use GLib `GRegex`** (PCRE2; part of GLib, so no new package); later also for search. `std::regex` is not used.
- 2026-11-07 — (Adam) **ctxmon is finished during M2**: settings M2.7, I/O on the main loop M2.8, cards and graphs M2.9, standalone `vermilion-ctxmon` M2.10; the CTXMon tab is M3.6. ctxmon moves from v0.4 to v0.1.
- 2026-11-07 — (Adam) **The Orchestrator does every documentation sweep.** README.md is brought up to date when Adam says "Handoff to tomorrow's session" (end of the development day), not after each change. AI_ONBOARDING.md section 6.2.
- 2026-11-07 — (Adam) **All references to the earlier code base are removed** from the files and the Project docs: `docs/archive/` deleted, the sidebar requirement rewritten as a plain specification, `claude/GATE0_ANALYSIS.md` deleted from the Project. Git history is kept as it is (no rewrite).
- 2026-11-07 — (Orchestrator, within the gate-size rule) AGENTS.md rule 20 (PRIVATE flag targets, `GRegex`, string comparison in tests) and three technical traps in AI_ONBOARDING.md section 5. M1.0 ports `fmt_duration` as its smoke test; fixtures are added gate by gate, each with one oracle generator.
- 2026-11-07 — M1.0 (`vermilion_ctxmon_core`, oracle script, `fmt_duration`, 69 tests) executed by Qwen, audited PASS WITH NOTES: evidence headers at `f39251b` with mtimes after the edits; flags as intended (`-fno-exceptions` on `format.cpp`, not on the test); reference hash unchanged. Note: the oracle wrote `reference/ctxmon/__pycache__` and Qwen removed it with `rm -rf` unasked; fix 1 makes the oracle write no bytecode.
- 2026-11-07 — M1.0 fix 1 (oracle sets `sys.dont_write_bytecode`) audited PASS: fixture hash unchanged, `reference/ctxmon` clean. (Adam) **Persistent no-cleanup rule**: AGENTS.md rule 8 now forbids deleting, moving or cleaning up any file the prompt does not name, byproducts included; reports list them under "Unexpected files". AI_ONBOARDING.md section 5 records the drift.
- 2026-11-07 — (Adam) **UI specifications first**: before the first UI work of any kind (M2.1 window, M2.9 ctxmon cards, every later panel), the Orchestrator tells Adam and discusses his UI specifications before writing a prompt. Marked in `docs/roadmap.md` under M2.
- 2026-11-07 — M1.1 (`log_tokens`: `tail_start`, `last_token` on GRegex with `G_REGEX_RAW`; six fixture logs; 73 tests) executed by Qwen, audited PASS. Qwen did not paste E3 raw ("see tool output above"); from M1.2 on, `cat -n` of new files and `git diff` are FILE items (AI_ONBOARDING.md 2.8).
- 2026-11-07 — (Orchestrator) Dictated code is compiled and checked before it is issued: the cloud workspace has g++ 13, clang 18, GLib 2.80 and clang-format 18.1.3 (AI_ONBOARDING.md 2.8). M1.2's code was compiled with the project flags, formatted, and run against the oracle on its fixtures and on the 30 M1.1 cases.
- 2026-11-07 — (Adam) **No Python after the port**: gate M2.11 (after the standalone `vermilion-ctxmon`) converts the oracle's expected JSON into inline C++ case tables, deletes `reference/ctxmon/ctxmon_gtk.py`, `tools/ctxmon_oracle.py` and the expected JSON, and makes `SourceCheck.Repo` fail on any `.py` file. The Python stays until then as the oracle (M1) and the side-by-side reference for the cards (M2.9). Supersedes "decided at the M2 review".
- 2026-11-07 — (Adam) **The terminal is first built with ctxmon** (M2.9, VTE): it satisfies the terminal requirement. Whether the editor's bottom panel still gets terminal tabs (M4.5, reusing that widget) is open.
- 2026-11-07 — M1.2 (`timing`: `last_speeds`, `parse_requests`; `log_tokens` gains `token_matches`; four timing fixtures; 77 tests) executed by Qwen, audited PASS: all files byte-identical to the Orchestrator's pre-compiled, oracle-checked copies.
- 2026-11-07 — (Orchestrator) Process fact: a write to Adam's computer delivered a stale HANDOFF.md (the file was staged under a path used before), so commit `b97b1ac` carries an old HANDOFF.md; restored in the end-of-day commit. From now on every file sent to the computer gets a new staging name, and its content is checked on the computer (grep) before Adam commits.

## 2b. Coding Agent Profiles  *(edit as evidence accumulates)*
**Agent A: Qwen3.8-27B (local llama.cpp) — the llama.cpp coding agent for this project (confirmed by Adam 2026-11-04).** Setup as in WeatherAPP: Q4_K_M via llama.cpp (Vulkan, MTP drafting). In that project its profile was: accurate facts and line numbers, discloses omissions, unaltered raw blocks; weaknesses were soft classifications, verbosity, and occasionally "..." inside raw blocks. Re-profile on this codebase; its habits on C/C++ and GTK are unknown.
- Gate H0 (2026-11-04): PASS. Notes: wrote "(no output)" for empty command output instead of an empty raw block; chat paste lost code fences, so fencing was not verified.
- M0.2 to M0.5b2 (2026-11-05/06): six gates in a row PASS WITH NOTES, no FAIL. Raw outputs matched the Orchestrator's fresh re-runs every time; it reports prompt bugs honestly instead of silently working around them; recurring small slips are test-count arithmetic in prose and "(no output)" annotations for empty output. M0.5b2 report was clean (all proofs matched).
**Agent B: none.** Gemini Flash (Antigravity `agy acp`) was set up and passed a read-only smoke test (Gate A0), then dropped by Adam on 2026-11-04. Not used, not offered. Gemma remains permanently banned.
**Gemma: permanently banned (Adam).** Never use, suggest, benchmark, or prompt for it.
**Harness (from 2026-11-04)**: OpenCode 1.19.5 (`~/.opencode/bin/opencode`, ACP) launched by CLion's Air plugin via `~/.jetbrains/acp.json` (entry "OpenCode", args `acp`, both MCP options off). Versions: CLion 2026.2.4.1 (snap), Air plugin 262.8665.521, Agent Client Protocol plugin 262.11879.106. Model wiring is in the project-root `opencode.json`: provider `llama.cpp` at http://127.0.0.1:8080/v1, model key `qwen3.8-27b`, limits context 131072 / output 8192 (match `qwen-server` -c and -n). Permissions in the same file (opt-out policy from 2026-11-05): edit = allow; webfetch and websearch = deny; bash = `"*": "allow"` first, then deny for each of `git push`, `commit`, `cherry-pick`, `merge`, `rebase`, `revert`, `am`, `checkout`, `switch`, `restore`, `reset`, `clean`, `stash`, `update-ref`, written both as `git <cmd>*` and `git -* <cmd>*` (the second form catches `git -C dir <cmd>`; it is narrower than `git * <cmd>*`, which wrongly denied `git grep -n commit`). OpenCode: the last matching rule wins. Qwen can do anything else without a prompt; only Adam commits and pushes. **OpenCode's defaults are mostly allow, so never remove the permission block.** Verified: agent reads AGENTS.md, answers correctly, ends with the required phrase; `ls` prompted for approval (before the widening; `ls` no longer prompts). Not yet verified: that denying a prompt is honored, behaviour over multi-step gates, and behaviour after a context wipe. Observed quirk: command output displayed twice in the chat view (display only). The Air plugin's docs do not describe permissions for custom agents, so keep re-checking the approval prompts after plugin updates.
**Prompt sizing**: ~10 hits needing context per gate; counts-only for high-volume sweeps; at most 10 commands per tool call.

## 3. Backlog & Known Debt  *(edit freely)*
- Spikes: GtkSourceView on a 50 MB file (M2.0); gdb built-in DAP (M6.2); providers' usage-on-stream support (M5.1).
- Hardening later: agent shell confinement (Bubblewrap).
- Packaging decision (M8): `.deb` or AppImage; Flatpak excluded.
- Dropped and not scheduled: git integration, multi-cursor, theme system, SSH, session restore, indexed search, plugin hook.

## 4. Session Log  *(newest first, keep 10)*
### 2026-11-07 (late night) — M1.0 to M1.2, M1.3 written, evening handoff
- M1.0 (fix 1: oracle writes no bytecode), M1.1 and M1.2 all PASS and pushed (HEAD `b97b1ac`, 77 tests). Decisions: AGENTS.md rule 8 forbids cleaning up unnamed files; UI specifications are discussed before any UI gate; the terminal is first built with ctxmon (M2.9); all Python removed at M2.11, expected values becoming inline C++ tables. The Orchestrator now compiles and oracle-checks dictated code in the cloud workspace before issuing (json-glib is missing there, so M1.3 was checked with stand-ins). README swept at the end of the day; HANDOFF.md restored after a stale write. Open: commit and push the sweep; issue M1.3.
### 2026-11-07 (night) — M0 review, M1 replanned, cleanup
- Resumed at `e6a7ed0` (tree clean, pushed). M0 review held with Adam: M1 core only with an oracle gate M1.0; ctxmon developed here in `src/ctxmon/{core,ui}`, Python frozen; `GRegex`; ctxmon finished in M2; Orchestrator does all doc sweeps, README at "Handoff to tomorrow's session"; all references to the earlier code base removed (files and Project; history kept). Docs updated; M1.0 prompt written. Open: docs commit with `git rm -r docs/archive`, push, M1.0 run and audit.
### 2026-11-07 (evening) — M0.5c1, M0.5c2, M0 finished
- Resumed at `1011dac`. M0.5c1 PASS (66 tests, `6c4de85`) and M0.5c2 PASS (67 tests, `11d817c`), both first time with no notes; docs commits `4cb9ed7`, `f12da97`; all pushed. Adam had the Orchestrator do the M0 docs sweep (README.md, ARCHITECTURE.md, docs/roadmap.md) instead of a Qwen gate. Open: sweep and handoff commit and push; M0 milestone review in a new chat; then M1.1.
### 2026-11-06 (night, end) — Session handoff
- Resumed at `8aeb656`. M0.5d2 (fix 1, PASS, `f677f59`) and M0.5d3 (fix 1 failed on the Orchestrator's wrong clang-format key, fix 2 PASS, `f2d5166`) committed and pushed with docs (`27aa8c0`, `ab92c4f`). Adam set three process rules: milestone review (AI_ONBOARDING.md 2.7), evidence files (AGENTS.md 18), and format rules (AGENTS.md 19, AI_ONBOARDING.md 2.8). M0.5c split into c1 and c2; both prompts written. Open: handoff commit and push, M0.5c1, M0.5c2, M0 docs sweep, M0 review, M1.1.
### 2026-11-06 (night) — Session handoff
- Adam committed and pushed M0.5d1 (`bdd9817`) and the docs (`3dcf41b`); tree clean. M0.5d2 prompt filled in with `3dcf41b`, not yet issued. Open: handoff commit and push, M0.5d2 run and audit, M0.5d3, M0.5c, M0 docs sweep, M1.1.
### 2026-11-06 (evening, later) — M0.5d1 audit and fix, clang-format decision
- M0.5b2 and docs committed and pushed (`e74ec72`, `e17e47b`). M0.5d1 issued; first report FAIL (no raw output, over-long lines from the prompt), fix 1, then PASS (58 tests, window confirmed). Adam approved a clang-format ctest: M0.5d3 format pass added before M0.5c. M0.5d2 prompt written. Open: commits and push, M0.5d2, M0.5d3, M0.5c, M0 docs sweep, M1.1.
### 2026-11-06 (evening) — M0.5b2 audit and fix, no-carried-debt rule
- Resumed at `d242c73` (handoff uncommitted); Adam committed and pushed it as `4a6e3b3`. M0.5b2 issued, audited PASS WITH NOTES, fix 1 in the same Qwen session, then PASS (54 tests). Adam set the no-carried-debt rule with fix loops; AGENTS.md rule 17, AI_ONBOARDING.md and the roadmap (M0.5d1, M0.5d2) updated. M0.5d1 prompt written; M0.5c prompt stored with a purpose-comment check. Open: commits and push, M0.5d1, M0.5d2, M0.5c, M0 docs sweep, M1.1.
### 2026-11-05 (night, late) — M0.5b1 audit, CTXMon tab and standalone
- `8fec3b5` is HEAD (ctxmon original in `reference/ctxmon/`). M0.5b1 audited PASS WITH NOTES (49 tests). Adam decided CTXMon is a third right-hand tab and also a standalone program; docs updated. M0.5b2 prompt written. Open: M0.5b1 and docs commits and push, M0.5b2 run and audit, M0.5c, M1.
### 2026-11-05 (night, later) — M0.5a audit, dogfooding, M0.5b1
- Resumed at `81dd030`; old tags confirmed removed. Adam set the first dogfooding point at M4.3 (roadmap markers added). M0.5a report audited PASS WITH NOTES (34 tests). M0.5b split; M0.5b1 prompt written. Open: M0.5a and docs commits and push, M0.5b1 run and audit, then M0.5b2, M0.5c.
### 2026-11-05 (night) — Session handoff
- Adam confirmed `git ls-remote --tags origin` is empty and that `bafd9e2` is HEAD; M0.5a prompt not yet issued (Adam will run it next session). Open: handoff commit, tag cleanup (local), M0.5a run and audit, then M0.5b and M0.5c.
