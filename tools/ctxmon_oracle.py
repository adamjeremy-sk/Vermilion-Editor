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
