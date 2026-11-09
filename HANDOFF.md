# HANDOFF.md — Orchestrator Continuity Log
Maintained by the Orchestrator per AI_ONBOARDING.md §6. A new instance reads this after AI_ONBOARDING.md and ARCHITECTURE.md.

## 1. Current State  *(overwrite every update)*
| Field | Value |
|---|---|
| Updated | 2026-11-08 ~21:15 (M1.4a audited PASS; gate commit, docs commit and push pending with Adam) |
| FIRST ACTION on every new chat | **HANDOFF.md and AI_ONBOARDING.md live in the claude.ai Project (read them with the Projects tool, `claude/HANDOFF.md`), not on disk: a fresh chat's working directory is empty, so do not look for them there. Once they are read, and before any work on the repo, get access to the repo folder.** Call `get_device_info` first; if `connectedFolders` already lists `/home/adam/Projects/vermilion-editor`, no request is needed. Otherwise call `device_request_folder_access` for that path once (the grant is recursive). If it is denied or unanswered, do not repeat it: say so, and either ask Adam to attach the folder or fall back to gated Qwen prompts. Then work through `device_bash` (the folder is mounted at `$HOME/mnt/vermilion-editor`; read-only git with `git --no-optional-locks`). Note: `device_bash` runs in a VM on Adam's machine, so tool versions seen there (python 3.10, awk) are not Adam's host versions. Editing rights are unchanged: root orchestration docs and `docs/` only, never code. |
| Project | Vermilion Editor: native Linux text and code editor, C++17, GTK 4, GtkSourceView 5, built-in AI agent, build/debug/terminal, model monitor (ctxmon), GPL-3.0-or-later. Written from scratch. |
| Repo state | HEAD `86671be`, in sync with `origin/main`. Uncommitted: the M1.4a gate (`server_state.h/.cpp`, `server_state_test.cpp`, `live_speed.json`, `server_state.json`, modified oracle and two CMake files) plus ` M HANDOFF.md`, ` M docs/roadmap.md` (Orchestrator). Also an untracked folder `Claude outputs/` (a file the Orchestrator sent with SendUserFile landed in the repo folder): Adam deletes it, never commits it. Adam commits the gate as "M1.4a: ctxmon live speed and server state (87 tests)", then the docs as "docs: handoff after M1.4a", and pushes. |
| Version / HEAD | `86671be`. ctxmon core: `format` (`fmt_duration`), `log_tokens`, `timing`, `server_api`; M1.4a adds `server_state` (`live_speed`, `ServerState` without warnings). Oracle generators `fmt_duration`, `last_tokens`, `timing`, `http`, and after M1.4a `live_speed`, `server_state`. |
| Tests | 87, all passing (M1.4a). |
| Active task | **M1, ctxmon logic** (M1.0 to M1.3 done; M1.4a issued). M0 is closed and reviewed (`docs/milestones/M0-review.md`). |
| Gate position | M1.4a executed and audited PASS (uncommitted until Adam commits). Next: M1.4b (warnings). |
| Awaiting | Adam: delete `Claude outputs/`, commit M1.4a and the docs, push, send the new short HEAD. Then the Orchestrator writes M1.4b. |
| Last prompt issued | M1.4a (issued 2026-11-08, verbatim below; audited PASS). |
| Next action | On Adam's HEAD: write **M1.4b**: port `_warnings` (levels 80 and 95, hysteresis 10 points, first-update rule, time-to-full alert under `ETA_WARN_SECONDS`); the constructor gains the warn levels; `StateView` gains `alerts` and `level`; the `down` branch also resets `warned`, `eta_warned` and `first` (left out of M1.4a on purpose); split `ServerState::update` (91 lines after M1.4a, flagged by rule 16) into helpers such as the exact-versus-estimate classification; the oracle records alerts and level. Compile and oracle-check the dictated code first (driver plus mutation check, as for M1.4a). Then M1.5 (graph geometry), M1.6 (VRAM readers). Before any UI gate, talk with Adam about his UI specifications. README.md only at the end-of-day handoff. |
| Agent / runtime | Qwen3.8-27B (local llama.cpp, `qwen-server`, port 8080) driven by OpenCode 1.19.5 over ACP inside CLion's Air plugin. Qwen is the only coding agent. Gemma permanently banned. Fresh agent session per prompt; commit only at task end (gate end). Adam builds and tests. Adam codes only at home on this computer with the chat linked, so the Orchestrator can always read `evidence/`. |

**Open notes (no carried debt, Adam 2026-11-06; each is scheduled in a gate)**: none open in code. Accepted design, not debt: `value_or` needs copyable `T`; `key=` query parameters are deliberately not masked; rule A's authorization branch stops only at quotes, comma, CR and LF (values like `Basic xyz` contain spaces). Standing process facts: GLib 2.80 `g_object_add_weak_pointer` does not set the pointer on add (tests preset it); reports show raw failing build output first; `git diff` shows nothing for untracked files, so prompts ask for new files in full; prompts must keep every C++ line they dictate within 100 columns; Qwen replaced raw output with summaries twice (M0.5d1, M0.5d2), answered from M0.5c1 on by AGENTS.md rule 18 (evidence files the Orchestrator reads itself; Adam trusts Qwen not to fabricate them, the Orchestrator still checks headers and mtimes for staleness); clang-format is 18.1.3 on Adam's machine and not installed in the Orchestrator's VM; do not run dependent Orchestrator tool calls in parallel; **prompts and AGENTS.md never mention what the Orchestrator checks or reads (Adam, 2026-11-06)**; check clang-format option names against version 18 before dictating them. Prompt-authoring practices (format step, no dictated whitespace, header lines for new files, FILE evidence, silence about the Orchestrator's checks, tool-version check) are in AI_ONBOARDING.md section 2 item 8. Topics already collected for the M0 review: raw-output summarising and rule 18; format-style choice and rule 19 (the Orchestrator's wrong clang-format key); prompt bugs found by Qwen (`EXPECT_EQ` on `const char*`, over-long dictated lines); GLib weak-pointer quirk; gate sizes. Product notes: dogfooding markers in `docs/roadmap.md` (first at M4.3); CTXMon is a tab plus standalone `vermilion-ctxmon` (names proposed); GTK 4 has no tray icon.
**Open decisions (Adam's call)**: none block M0. Names of config files and JSON schemas are fixed when first written (ARCHITECTURE.md section 10); gdb DAP reliability is checked at M6.2; packaging at M8.

## Last prompt (verbatim)
FIRST: read AGENTS.md in the project root before doing anything else. Its rules are binding for this entire task and override anything below if they conflict. If you cannot read it, say so and STOP.
Last verified: New task; HEAD 86671be with exactly two uncommitted files, HANDOFF.md and docs/roadmap.md (documentation edits; they are expected, so do not touch, revert or build on them, and do not list them under E11). Otherwise the tree is clean.

TASK: Gate M1.4a, ctxmon live speed and server state. One step.

CONTEXT (BINDING)
- ctxmon logic lives in `src/ctxmon/core` (library `vermilion_ctxmon_core`, namespace `vermilion::ctxmon`, no GTK headers; GLib and json-glib are allowed). Tests live in `tests/ctxmon` (executable `vermilion_ctxmon_tests`). Expected values come from the frozen Python original `reference/ctxmon/ctxmon_gtk.py` (read-only) through `tools/ctxmon_oracle.py`.
- This gate ports `live_speed` and the class `ServerState` (ctxmon_gtk.py lines 275 to 290 and 410 to 532) as a new module `server_state`: the histories with pruning, the request history, the live speed, and the exact-versus-estimate token count. The warnings (`_warnings`, the 80% and 95% levels, the time-to-full alert) are NOT part of this gate; they come in M1.4b.
- The oracle feeds scripted snapshot sequences to the Python `ServerState` and records, after each step, what `update` returned and the three histories. Those scripts are defined in the oracle itself and written into `tests/ctxmon/fixtures/expected/server_state.json` together with the answers, so the C++ test reads its inputs from that file too.
- Test count: 83 now, 87 after this gate.

STRICT RAILS
- Create exactly these files: `src/ctxmon/core/server_state.h`, `src/ctxmon/core/server_state.cpp`, `tests/ctxmon/server_state_test.cpp`, and `tests/ctxmon/fixtures/expected/live_speed.json` and `tests/ctxmon/fixtures/expected/server_state.json` (both written by the oracle, never by hand).
- Modify exactly these files: `tools/ctxmon_oracle.py`, `src/ctxmon/core/CMakeLists.txt`, `tests/ctxmon/CMakeLists.txt`.
- Re-running the oracle rewrites the other four expected JSON files with identical content: `git status` must not list them as modified.
- No other change. No new dependency.

STEP 1
1. Replace the whole content of `tools/ctxmon_oracle.py` with exactly:
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


def live_speed_cases(m):
    inputs = [
        ([], None),
        ([[0.0, 5]], None),
        ([[0.0, 5], [1.0, 5]], None),
        ([[0.0, 5], [1.0, 25]], None),
        ([[0.0, 5], [1.0, 3]], None),
        ([[1.0, 5], [1.0, 25]], None),
        ([[0.0, 0], [0.5, 10], [1.0, 30], [2.0, 70]], None),
        ([[0.0, 0], [0.5, 10], [1.0, 30], [2.5, 70]], None),
        ([[0.0, 0], [0.5, 10], [1.0, 30], [3.0, 70]], None),
        ([[0.0, 0], [0.5, 10], [4.0, 30], [10.0, 90]], None),
        ([[0.0, 0], [4.0, 30], [4.5, 30]], None),
        ([[0.0, 0], [1.0, 10], [2.0, 30]], 1.0),
        ([[0.0, 0], [1.0, 10], [2.0, 30]], 5.0),
    ]
    cases = []
    for samples, window in inputs:
        args = [[(t, n) for t, n in samples]] + ([] if window is None else [window])
        cases.append({"samples": samples, "window": window, "speed": m.live_speed(*args)})
    return cases


def snap(**kw):
    base = dict(state="up", used=None, used_kind=None, n_ctx=None, busy=False, n_decoded=None,
                model=None, log_found=True, gen=None, prompt=None, requests=[])
    base.update(kw)
    return base


def req(gen_tps, gen_tokens=100, prompt_tokens=None, prompt_tps=None, ctx=None, seed=False):
    return dict(prompt_tokens=prompt_tokens, prompt_tps=prompt_tps, gen_tokens=gen_tokens,
                gen_tps=gen_tps, ctx=ctx, seed=seed)


def run_ticks(start, step, count, make):
    return [(round(start + i * step, 6), make(i)) for i in range(count)]


def state_scenarios():
    busy = dict(used=2000, used_kind="prompt", n_ctx=8192, busy=True)
    big = dict(n_ctx=2000000, used=1234567, used_kind="prompt", busy=True)
    return [
        {"name": "lifecycle", "default_ctx": 8192, "window": 600, "full": True, "steps": [
            (0.0, snap(state="slow")),
            (1.0, snap(state="down")),
            (2.0, snap()),
            (3.0, snap(used=1200, used_kind="final", n_ctx=8192, model="qwen", gen=41.5,
                       prompt=900.25, requests=[req(41.5, 300, 1200, 900.25, 1200)])),
            (4.0, snap(used=1200, used_kind="final", n_ctx=8192, busy=True, n_decoded=0)),
            (5.0, snap(**busy, n_decoded=0)),
            (6.0, snap(**busy, n_decoded=10)),
            (6.5, snap(**busy, n_decoded=30)),
            (7.0, snap(**busy, n_decoded=50)),
            (7.5, snap(state="slow")),
            (8.0, snap(**busy, n_decoded=90)),
            (9.0, snap(used=2400, used_kind="final", n_ctx=8192,
                       requests=[req(40.0, 400, 2000, 800.0, 2400)])),
            (10.0, snap(state="down")),
            (11.0, snap(used=2400, used_kind="final", n_ctx=4096)),
        ]},
        {"name": "no_request_yet", "default_ctx": 4096, "window": 600, "full": True, "steps": [
            (0.0, snap(busy=True, n_decoded=0)),
            (1.0, snap(busy=True, n_decoded=5)),
            (2.0, snap(busy=True, n_decoded=1500)),
            (3.0, snap(busy=None)),
        ]},
        {"name": "kinds_while_busy", "default_ctx": 8192, "window": 600, "full": True, "steps": [
            (0.0, snap(used=500, used_kind="past", n_ctx=8192, busy=True, n_decoded=0)),
            (1.0, snap(used=500, used_kind="past", n_ctx=8192, busy=True, n_decoded=40)),
            (2.0, snap(used=500, used_kind="final", n_ctx=8192, busy=True, n_decoded=80)),
            (3.0, snap(used=500, used_kind=None, n_ctx=8192, busy=True, n_decoded=120)),
            (4.0, snap(used=500, used_kind="final", n_ctx=8192, busy=None, n_decoded=120)),
        ]},
        {"name": "full_context", "default_ctx": 4096, "window": 600, "full": True, "steps": [
            (0.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (1.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=100)),
            (2.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=300)),
            (3.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=1096)),
            (4.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=1300)),
        ]},
        {"name": "big_numbers", "default_ctx": 100, "window": 600, "full": True, "steps": [
            (0.0, snap(**big, n_decoded=0)),
            (1.0, snap(**big, n_decoded=999)),
            (2.0, snap(**big, n_decoded=1000)),
            (3.0, snap(n_ctx=2000000, used=1000000, used_kind="final")),
            (4.0, snap(n_ctx=2000000, used=999999, used_kind="final")),
        ]},
        {"name": "default_context", "default_ctx": 0, "window": 600, "full": True, "steps": [
            (0.0, snap(used=100, used_kind="final")),
            (1.0, snap(used=100, used_kind="final", n_ctx=0)),
            (2.0, snap(state="down")),
        ]},
        {"name": "new_request_resets_samples", "default_ctx": 8192, "window": 600, "full": True,
         "steps": [
            (0.0, snap(**busy, n_decoded=0)),
            (0.5, snap(**busy, n_decoded=100)),
            (1.0, snap(**busy, n_decoded=200)),
            (1.5, snap(**busy, n_decoded=5)),
            (2.0, snap(**busy, n_decoded=60)),
            (2.5, snap(**busy, n_decoded=None)),
            (3.0, snap(**busy, n_decoded=70)),
        ]},
        {"name": "speed_history_spacing", "default_ctx": 8192, "window": 600, "full": True,
         "steps": [
            (0.0, snap(used=100, used_kind="final", requests=[req(30.0)])),
            (0.4, snap(used=100, used_kind="final", requests=[req(31.0)])),
            (1.0, snap(used=100, used_kind="final", requests=[req(32.0)])),
            (1.2, snap(used=100, used_kind="final", requests=[req(0.0)])),
            (1.4, snap(used=100, used_kind="final", requests=[req(33.0, seed=True)])),
            (3.0, snap(**busy, n_decoded=0)),
            (3.5, snap(**busy, n_decoded=20)),
            (4.0, snap(**busy, n_decoded=60)),
            (4.5, snap(used=2100, used_kind="final", n_ctx=8192)),
            (5.0, snap(used=2100, used_kind="final", n_ctx=8192)),
            (6.0, snap(state="down")),
            (7.0, snap(state="down")),
        ]},
        {"name": "pruning", "default_ctx": 8192, "window": 10, "full": True, "steps": run_ticks(
            0.0, 2.5, 14, lambda i: snap(used=100 + 50 * (i // 2), used_kind="final",
                                         requests=[req(20.0 + i)] if i % 3 == 0 else []))},
        {"name": "request_history_cap", "default_ctx": 8192, "window": 600, "full": True,
         "steps": [
            (0.0, snap(used=10, used_kind="final",
                       requests=[req(float(i + 1), seed=(i % 4 == 0)) for i in range(55)])),
            (1.0, snap(used=10, used_kind="final", requests=[req(70.0), req(71.0)])),
        ]},
        {"name": "context_history_spacing", "default_ctx": 8192, "window": 600, "full": True,
         "steps": [(t, snap(used=500, used_kind="final", n_ctx=8192))
                   for t in (0.0, 1.0, 2.0, 3.0, 4.0, 6.25, 7.0, 8.0, 8.5)]},
        {"name": "sample_cap", "default_ctx": 100000, "window": 600, "full": False,
         "steps": run_ticks(0.0, 0.02, 100, lambda i: snap(
             used=1000, used_kind="prompt", n_ctx=100000, busy=True,
             n_decoded=10 * i + i * i // 10))},
    ]


def view_of(result):
    out = {"state": result["state"]}
    if result["state"] == "slow":
        return out
    for key in ("ctx", "shown", "frac", "kind", "source", "estimating", "live", "gen", "prompt",
                "eta", "busy", "model"):
        out[key] = result[key]
    out["log_found"] = bool(result.get("log_found"))
    return out


def histories_of(state):
    return {"ctx_hist": [list(p) for p in state.ctx_hist],
            "speed_hist": [list(p) for p in state.speed_hist],
            "requests": [dict(r) for r in state.requests]}


def server_state_cases(m):
    cases = []
    for scenario in state_scenarios():
        state = m.ServerState(scenario["default_ctx"], history_seconds=scenario["window"])
        steps = []
        last = len(scenario["steps"]) - 1
        for i, (now, snapshot) in enumerate(scenario["steps"]):
            step = {"now": now, "snapshot": snapshot}
            step["view"] = view_of(state.update(snapshot, now))
            if scenario["full"] or i == last:
                step.update(histories_of(state))
            steps.append(step)
        cases.append({"name": scenario["name"], "default_ctx": scenario["default_ctx"],
                      "window": scenario["window"], "steps": steps})
    return cases


GENERATORS = {
    "fmt_duration": fmt_duration_cases,
    "last_tokens": last_tokens_cases,
    "http": http_cases,
    "live_speed": live_speed_cases,
    "server_state": server_state_cases,
    "timing": timing_cases,
}


def server_state_text(cases):
    """One scenario per block and one step per line, so the file stays readable and diffable."""

    def line(value):
        return json.dumps(value, separators=(",", ":"))

    out = ["["]
    for ci, case in enumerate(cases):
        head = line({k: case[k] for k in ("name", "default_ctx", "window")})
        out.append(head[:-1] + ',"steps":[')
        for si, step in enumerate(case["steps"]):
            out.append(line(step) + ("," if si < len(case["steps"]) - 1 else ""))
        out.append("]}" + ("," if ci < len(cases) - 1 else ""))
    out.append("]")
    return "\n".join(out) + "\n"


WRITERS = {"server_state": server_state_text}


def main():
    m = load_original()
    EXPECTED.mkdir(parents=True, exist_ok=True)
    for name, generate in sorted(GENERATORS.items()):
        path = EXPECTED / (name + ".json")
        cases = generate(m)
        write = WRITERS.get(name)
        text = write(cases) if write else json.dumps(cases, indent=2) + "\n"
        path.write_text(text, encoding="utf-8")
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
```
2. Run: `python3 tools/ctxmon_oracle.py; echo "exit=$?"`
3. Create `src/ctxmon/core/server_state.h` with exactly:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// Turns server snapshots into what the monitor shows and keeps the histories.

#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "ctxmon/core/log_tokens.h"
#include "ctxmon/core/timing.h"

namespace vermilion::ctxmon {

constexpr double kLiveWindow = 2.0;      // seconds of samples used for the live speed
constexpr std::size_t kMaxRequests = 50; // request history length per server
constexpr std::size_t kMaxSamples = 64;  // live-speed samples kept per server

// How many tokens a running request has generated, and when that was read.
struct Sample {
    double time = 0.0;
    long long n_decoded = 0;
};

// live_speed of the Python original: tokens per second from the samples within window seconds
// of the newest one. Absent with fewer than two samples in the window or without growth.
std::optional<double> live_speed(const std::deque<Sample>& samples, double window = kLiveWindow);

// Whether the server answered: Up, Down (nothing listening) or Slow (no timely answer, which a
// busy server can give, so it is not treated as down).
enum class LinkState { Up, Down, Slow };

// A finished request handed over with a snapshot. seed is true for history that was already in
// the log when the monitor started, so its time is unknown.
struct SnapshotRequest {
    FinishedRequest request;
    bool seed = false;
};

// Everything one poll found, as the Python original's poll_once returns it.
struct Snapshot {
    LinkState state = LinkState::Up;
    std::optional<long long> used; // newest token count in the log
    std::optional<TokenKind> used_kind;
    std::optional<long long> n_ctx;
    bool busy = false;
    std::optional<long long> n_decoded;
    std::optional<std::string> model;
    bool log_found = false;
    std::optional<double> gen;    // tokens per second of the latest finished request
    std::optional<double> prompt; // prompt speed of the latest finished request
    std::vector<SnapshotRequest> requests;
};

// How trustworthy the shown token count is.
enum class CountKind {
    Down,       // the server is down
    None,       // no request in the log yet
    Exact,      // idle, and the log says how many tokens the context holds
    Processing, // a request is loading its prompt: the count is the last known one
    Estimate,   // a request is generating: prompt size plus what /slots reports
};

// The name the Python original uses for a kind ("down", "none", "exact", "processing",
// "estimate").
const char* count_kind_name(CountKind kind);

// What update returns. With state Slow nothing else is set: the caller keeps the old numbers.
struct StateView {
    LinkState state = LinkState::Up;
    long long ctx = 0;
    long long shown = 0;
    double frac = 0.0; // shown / ctx, at most 1
    CountKind kind = CountKind::None;
    std::string source; // says which parts of the count are known
    bool estimating = false;
    std::optional<double> live; // live generation speed
    std::optional<double> gen;
    std::optional<double> prompt;
    std::optional<double> eta; // seconds until the context is full at the live speed
    bool busy = false;
    std::optional<std::string> model;
    bool log_found = false;
};

struct CtxPoint {
    double time = 0.0;
    long long tokens = 0;
};

struct SpeedPoint {
    double time = 0.0;
    std::optional<double> tps; // absent: a gap between requests
};

struct RequestRecord {
    FinishedRequest request;
    std::optional<double> time; // absent for seed requests
};

// ServerState of the Python original, without the warnings (they come in the next gate).
class ServerState {
public:
    explicit ServerState(long long default_ctx, double history_seconds = 600.0)
        : default_ctx_(default_ctx), window_(history_seconds) {}

    // Digests one snapshot taken at time now (seconds).
    StateView update(const Snapshot& snapshot, double now);

    const std::deque<CtxPoint>& ctx_history() const { return ctx_hist_; }
    const std::deque<SpeedPoint>& speed_history() const { return speed_hist_; }
    const std::deque<RequestRecord>& requests() const { return requests_; }

private:
    void push_ctx(double now, long long tokens);
    void push_speed(double now, std::optional<double> tps);
    void add_requests(const std::vector<SnapshotRequest>& requests, double now);

    long long default_ctx_;
    double window_;
    std::deque<CtxPoint> ctx_hist_;
    std::deque<SpeedPoint> speed_hist_;
    std::deque<RequestRecord> requests_;
    std::deque<Sample> samples_;
    long long last_ctx_ = 0; // 0: none yet
    bool was_busy_ = false;
};

} // namespace vermilion::ctxmon
```
4. Create `src/ctxmon/core/server_state.cpp` with exactly:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
// ServerState of the Python original: histories, exact versus estimated token counts.

#include "ctxmon/core/server_state.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace vermilion::ctxmon {

namespace {

// n with comma thousands separators, like Python's f"{n:,}".
std::string with_commas(long long n) {
    const bool negative = n < 0;
    const unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(n) : static_cast<unsigned long long>(n);
    const std::string digits = std::to_string(magnitude);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    return negative ? "-" + out : out;
}

// Drops points from the front while the second one is older than the window: one point older
// than the window stays, so lines enter from the left edge.
template <typename History>
void prune(History& history, double now, double window) {
    while (history.size() > 1 && history[1].time < now - window) {
        history.pop_front();
    }
}

} // namespace

std::optional<double> live_speed(const std::deque<Sample>& samples, double window) {
    if (samples.size() < 2) {
        return std::nullopt;
    }
    const double t_end = samples.back().time;
    const Sample* first = nullptr;
    for (const Sample& s : samples) {
        if (t_end - s.time <= window) {
            first = &s;
            break;
        }
    }
    const Sample& last = samples.back();
    if (first == nullptr || first == &last) {
        return std::nullopt; // fewer than two samples in the window
    }
    if (last.time <= first->time || last.n_decoded <= first->n_decoded) {
        return std::nullopt;
    }
    return static_cast<double>(last.n_decoded - first->n_decoded) / (last.time - first->time);
}

const char* count_kind_name(CountKind kind) {
    switch (kind) {
    case CountKind::Down:
        return "down";
    case CountKind::None:
        return "none";
    case CountKind::Exact:
        return "exact";
    case CountKind::Processing:
        return "processing";
    case CountKind::Estimate:
        return "estimate";
    }
    return "";
}

void ServerState::push_ctx(double now, long long tokens) {
    if (ctx_hist_.empty() || tokens != ctx_hist_.back().tokens ||
        now - ctx_hist_.back().time >= 2.0) {
        ctx_hist_.push_back({now, tokens});
    }
    prune(ctx_hist_, now, window_);
}

void ServerState::push_speed(double now, std::optional<double> tps) {
    if (!tps) {
        if (!speed_hist_.empty() && speed_hist_.back().tps) {
            speed_hist_.push_back({now, std::nullopt});
        }
    } else if (speed_hist_.empty() || !speed_hist_.back().tps ||
               now - speed_hist_.back().time >= 1.0) {
        speed_hist_.push_back({now, tps});
    }
    prune(speed_hist_, now, window_);
}

void ServerState::add_requests(const std::vector<SnapshotRequest>& requests, double now) {
    for (const SnapshotRequest& r : requests) {
        requests_.push_back({r.request, r.seed ? std::nullopt : std::optional<double>(now)});
        if (requests_.size() > kMaxRequests) {
            requests_.pop_front();
        }
        if (!r.seed && r.request.gen_tps != 0.0) {
            push_speed(now, r.request.gen_tps);
        }
    }
}

StateView ServerState::update(const Snapshot& s, double now) {
    StateView v;
    v.state = s.state;
    if (s.state == LinkState::Slow) {
        return v;
    }
    add_requests(s.requests, now);

    if (s.state == LinkState::Down) {
        v.ctx = last_ctx_ != 0 ? last_ctx_ : default_ctx_;
        v.kind = CountKind::Down;
        push_ctx(now, 0);
        push_speed(now, std::nullopt);
        samples_.clear();
        was_busy_ = false;
        return v;
    }

    const long long ctx = s.n_ctx && *s.n_ctx != 0 ? *s.n_ctx : default_ctx_;
    last_ctx_ = ctx;

    // Live speed from the tokens generated so far by the running request.
    std::optional<double> live;
    if (s.busy && s.n_decoded) {
        if (!samples_.empty() && *s.n_decoded < samples_.back().n_decoded) {
            samples_.clear(); // a new request started
        }
        samples_.push_back({now, *s.n_decoded});
        if (samples_.size() > kMaxSamples) {
            samples_.pop_front();
        }
        live = live_speed(samples_);
    } else {
        samples_.clear();
    }
    if (live) {
        push_speed(now, live);
    } else if (was_busy_ && !s.busy) {
        push_speed(now, std::nullopt); // break the line between requests
    }
    was_busy_ = s.busy;

    // Exact versus estimate.
    const long long gen_now = s.busy && s.n_decoded && *s.n_decoded != 0 ? *s.n_decoded : 0;
    if (!s.used) {
        v.kind = CountKind::None;
        v.shown = gen_now;
        v.estimating = gen_now != 0;
        v.source = v.estimating ? "estimate: " + with_commas(gen_now) +
                                      " generated, prompt size not in the log yet"
                                : "no request in the log yet";
    } else if (!s.busy) {
        v.kind = CountKind::Exact;
        v.shown = *s.used;
        v.source = "exact: from the server log";
    } else if (s.used_kind == TokenKind::Final && gen_now == 0) {
        // Busy, but the newest log line is the previous request's end: its prompt is not known.
        v.kind = CountKind::Processing;
        v.shown = *s.used;
        v.estimating = true;
        v.source = "estimate: new prompt loading (last known " + with_commas(*s.used) + ")";
    } else if (gen_now == 0) {
        v.kind = CountKind::Processing;
        v.shown = *s.used;
        v.estimating = true;
        v.source = "estimate: prompt loading, " + with_commas(*s.used) + " so far";
    } else {
        v.kind = CountKind::Estimate;
        v.shown = *s.used + gen_now;
        v.estimating = true;
        v.source = "estimate: prompt " + with_commas(*s.used) + " + " + with_commas(gen_now) +
                   " generated";
    }

    v.ctx = ctx;
    v.frac =
        ctx != 0 ? std::min(static_cast<double>(v.shown) / static_cast<double>(ctx), 1.0) : 0.0;
    const long long free_tokens = std::max(ctx - v.shown, 0LL);
    if (s.busy && live && free_tokens > 0) {
        v.eta = static_cast<double>(free_tokens) / *live;
    }
    push_ctx(now, v.shown);

    v.live = live;
    v.gen = s.gen;
    v.prompt = s.prompt;
    v.busy = s.busy;
    v.model = s.model;
    v.log_found = s.log_found;
    return v;
}

} // namespace vermilion::ctxmon
```
5. In `src/ctxmon/core/CMakeLists.txt`, change the first line to `add_library(vermilion_ctxmon_core STATIC format.cpp log_tokens.cpp timing.cpp server_api.cpp server_state.cpp)`.
6. In `tests/ctxmon/CMakeLists.txt`, change the first line to `add_executable(vermilion_ctxmon_tests format_test.cpp log_tokens_test.cpp timing_test.cpp server_api_test.cpp server_state_test.cpp)`.
7. Create `tests/ctxmon/server_state_test.cpp` with exactly:
```cpp
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
```
8. Run: `clang-format -i src/ctxmon/core/server_state.h src/ctxmon/core/server_state.cpp tests/ctxmon/server_state_test.cpp; echo "exit=$?"`
9. Configure, build and test. Each command writes a FILE in the exact form of AGENTS.md rule 18, gate `M1.4a`:
    - `evidence/M1.4a/configure.txt` from `cmake -S . -B build`
    - `evidence/M1.4a/build.txt` from `cmake --build build`
    - `evidence/M1.4a/ctest.txt` from `ctest --test-dir build --output-on-failure`

EVIDENCE (AGENTS.md rules 12 and 18; FILE items are written by the command itself, then only their `tail -n 3` is pasted)
E1. Raw: `git status --short --untracked-files=all`
E2. FILE `evidence/M1.4a/diff.txt` from `git diff`
E3. FILE `evidence/M1.4a/new_files.txt` from `cat -n src/ctxmon/core/server_state.h src/ctxmon/core/server_state.cpp tests/ctxmon/server_state_test.cpp`
E4. Raw: `wc -lc tests/ctxmon/fixtures/expected/*.json`
E5. Raw: `grep -o '^{"name":"[a-z_]*"' tests/ctxmon/fixtures/expected/server_state.json`
E6. Raw: the output of steps 2 and 8.
E7. Raw: `wc -l` of every created or modified file except the two expected JSON files.
E8. Raw: `grep -rnE "gtk/|gdk|pango|gtksourceview|vte|std::regex" src/ctxmon/core; echo "exit=$?"`
E9. For each of the five evidence files: its path, then the raw output of `tail -n 3` on it.
E10. Raw: `grep -E "tests passed|tests failed" evidence/M1.4a/ctest.txt`
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
- 2026-11-08 — (Adam) **Terminal**: the ctxmon terminal (VTE, M2.9) is good enough as Vermilion's terminal; M4.5 (terminal tabs in the editor's bottom panel) is built only if that terminal proves burdensome to use. Review trigger: burdensome in daily use.
- 2026-11-08 — M1.3 (`server_api`: `parse_slots`, `model_from_props`, `model_from_models` on json-glib; 17 http fixtures; oracle generator `http`; 83 tests) executed by Qwen, audited PASS: evidence headers at `f3b2b8e`, mtimes after the edits, all four code files and all 17 fixtures byte-identical to the prompt, `http.json` values as expected, other expected JSON unmodified, no GTK names in ctxmon core. The first attempt stopped correctly because the prompt said "tree clean" while two Orchestrator doc edits were uncommitted; a continuation prompt named them. Lesson: when the Orchestrator edits docs before issuing a prompt, the "Last verified" line lists those files.
- 2026-11-08 — (Orchestrator, within the gate-size rule) **M1.4 split**: M1.4a `live_speed` and `ServerState` without warnings, M1.4b the warnings. The oracle defines the scripted snapshot sequences itself and writes inputs and answers together into `server_state.json` (one step per line, so the 100-step sample-cap scenario stays reviewable); `live_speed.json` holds the window cases. Process: the Orchestrator ran a C++ driver against the Python original and mutation-tested it before issuing; the "Last verified" line of a prompt lists uncommitted Orchestrator doc edits.
- 2026-11-08 — M1.4a (`server_state`: `live_speed`, `ServerState` without warnings; oracle generators `live_speed` and `server_state`, 12 scripted scenarios; 87 tests) executed by Qwen, audited PASS: evidence headers at `86671be`, mtimes after the edits, all four code files byte-identical to the prompt, CMake lines as dictated, the two new expected files byte-identical (sha256) to the ones the Orchestrator generated and tested against a C++ driver, four older expected files unmodified. Qwen flagged `ServerState::update` at 91 lines (rule 16 flag, justified as one pass); M1.4b splits it. Process: a file sent with SendUserFile landed in the repo folder as `Claude outputs/prompt_M1.4a.txt`; Qwen left it alone (rule 8) and listed it; how long prompts are delivered (inline in the reply, or as a sent file that Adam deletes from the repo folder afterwards) is Adam's choice.

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
### 2026-11-08 (evening) — M1.3
- Resumed at `f3b2b8e` (clean, synced). Adam: the ctxmon terminal is good enough as the terminal; M4.5 only if it proves burdensome. M1.3 issued, first stopped by my "tree clean" error, then run and audited PASS (83 tests). M1.3 committed (`cfd1179`) and pushed (`86671be`). M1.4 split; M1.4a prompt issued. Open: M1.4a run and audit, commits, push, M1.4b.

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
