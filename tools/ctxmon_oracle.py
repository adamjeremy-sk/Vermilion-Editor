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
        {"name": "warning_levels", "default_ctx": 1000, "window": 600, "full": False, "steps": [
            (float(i), snap(used=used, used_kind="final", n_ctx=1000))
            for i, used in enumerate([900, 920, 960, 960, 850, 840, 700, 690, 810, 970, 985])]},
        {"name": "jump_over_both_levels", "default_ctx": 1000, "window": 600, "full": False,
         "steps": [(0.0, snap(used=100, used_kind="final", n_ctx=1000)),
                   (1.0, snap(used=970, used_kind="final", n_ctx=1000)),
                   (2.0, snap(used=100, used_kind="final", n_ctx=1000)),
                   (3.0, snap(used=800, used_kind="final", n_ctx=1000))]},
        {"name": "down_resets_warnings", "default_ctx": 1000, "window": 600, "full": False,
         "steps": [(0.0, snap(used=900, used_kind="final", n_ctx=1000)),
                   (1.0, snap(used=960, used_kind="final", n_ctx=1000)),
                   (2.0, snap(state="down")),
                   (3.0, snap(used=960, used_kind="final", n_ctx=1000)),
                   (4.0, snap(used=650, used_kind="final", n_ctx=1000)),
                   (5.0, snap(used=850, used_kind="final", n_ctx=1000))]},
        {"name": "percent_rounds_half_even", "default_ctx": 8000, "levels": [10],
         "window": 600, "full": False,
         "steps": [(0.0, snap(used=80, used_kind="final", n_ctx=8000)),
                   (1.0, snap(used=1000, used_kind="final", n_ctx=8000))]},
        {"name": "percent_rounds_up", "default_ctx": 8000, "levels": [13], "window": 600,
         "full": False,
         "steps": [(0.0, snap(used=80, used_kind="final", n_ctx=8000)),
                   (1.0, snap(used=1080, used_kind="final", n_ctx=8000))]},
        {"name": "unsorted_levels", "default_ctx": 1000, "levels": [95, 50, 80], "window": 600,
         "full": False,
         "steps": [(0.0, snap(used=100, used_kind="final", n_ctx=1000)),
                   (1.0, snap(used=600, used_kind="final", n_ctx=1000)),
                   (2.0, snap(used=990, used_kind="final", n_ctx=1000))]},
        {"name": "no_levels", "default_ctx": 1000, "levels": [], "window": 600, "full": False,
         "steps": [(0.0, snap(used=100, used_kind="final", n_ctx=1000)),
                   (1.0, snap(used=990, used_kind="final", n_ctx=1000))]},
        {"name": "time_to_full_alert", "default_ctx": 4096, "window": 600, "full": False,
         "steps": [
            (0.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (1.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=100)),
            (2.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=200)),
            (3.0, snap(used=3200, used_kind="final", n_ctx=4096)),
            (4.0, snap(used=3200, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (5.0, snap(used=3200, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=300)),
            (6.0, snap(used=3200, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=600)),
            (7.0, snap(used=100, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (8.0, snap(used=100, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=5)),
            (9.0, snap(used=100, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=8)),
        ]},
        {"name": "hysteresis_boundary", "default_ctx": 1000, "levels": [95], "window": 600,
         "full": False,
         "steps": [(float(i), snap(used=used, used_kind="final", n_ctx=1000))
                   for i, used in enumerate([900, 960, 855, 960, 850, 960, 849, 960])]},
        {"name": "time_to_full_after_down", "default_ctx": 4096, "window": 600, "full": False,
         "steps": [
            (0.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (1.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=100)),
            (2.0, snap(state="down")),
            (3.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=0)),
            (4.0, snap(used=3000, used_kind="prompt", n_ctx=4096, busy=True, n_decoded=100)),
        ]},
        {"name": "sample_cap", "default_ctx": 100000, "window": 600, "full": False,
         "steps": run_ticks(0.0, 0.02, 100, lambda i: snap(
             used=1000, used_kind="prompt", n_ctx=100000, busy=True,
             n_decoded=10 * i + i * i // 10))},
    ]


GRAPH_SERIES = {
    "empty": [],
    "single": [(95, 5)],
    "rising": [(60, 0), (70, 10), (80, 20), (90, 30), (100, 40)],
    "with_gaps": [(60, 10), (70, 20), (80, None), (90, 30), (95, None), (100, 50)],
    "old_then_new": [(10, 7), (20, 9), (50, 12), (70, 15), (90, 20)],
    "old_gap_new": [(10, 3), (20, None), (30, 4), (90, 5)],
    "leading_gaps": [(50, None), (60, 5), (70, None), (80, None), (90, 6)],
    "trailing_gap_now": [(60, 5), (70, 6), (100, None)],
    "trailing_gap_future": [(60, 5), (120, None)],
    "out_of_range": [(70, -5), (80, 500), (90, 12.5)],
    "future_points": [(90, 3), (110, 4)],
    "same_times": [(80, 1), (80, 2)],
}

# (now, span, ymax, w, h): normal sizes, a long span with fractional sizes, a short span
GRAPH_FRAMES = [(100, 60, 50, 300, 100), (100, 600, 100, 99.5, 47.25), (100, 30, 20, 200, 80)]

# degenerate frames: no or negative span, ceiling, width and height
GRAPH_EMPTY_FRAMES = [(100, 0, 50, 300, 100), (100, 60, 0, 300, 100), (100, 60, 50, 0, 100),
                      (100, 60, 50, 300, 0), (100, -60, 50, 300, 100), (100, 60, -1, 300, 100),
                      (100, 60, 50, -300, 100), (100, 60, 50, 300, -1)]


def graph_segments_cases(m):
    cases = []
    jobs = [(name, frame) for name in GRAPH_SERIES for frame in GRAPH_FRAMES]
    jobs += [("rising", frame) for frame in GRAPH_EMPTY_FRAMES]
    for name, (now, span, ymax, w, h) in jobs:
        points = GRAPH_SERIES[name]
        for step in (False, True):
            segments = m.graph_segments(points, now, span, ymax, w, h, step)
            cases.append({"series": name, "points": [list(p) for p in points], "now": now,
                          "span": span, "ymax": ymax, "w": w, "h": h, "step": step,
                          "segments": [[list(p) for p in seg] for seg in segments]})
    return cases


def nice_max_cases(m):
    values = [0, 1, 9.99, 10, 10.0001, 11, 19, 20, 21, 49.9, 50, 51, 99, 100, 101, 249, 250,
              251, 499.5, 500, 501, 999, 1000, 1001, 2499, 2500, 2501, 2999, 3000, 12345.6,
              9.999999999999998, 10.000000000000002, 50.00000000000001, 100.00000000000001,
              250.00000000000003, 1e6, -5, 0.1 + 0.2]
    cases = [{"v": v, "minimum": None, "result": m.nice_max(v)} for v in values]
    for v, minimum in [(5, 20), (30, 100), (0, 0), (-5, -10), (7.5, 7), (2600, 3000)]:
        cases.append({"v": v, "minimum": minimum, "result": m.nice_max(v, minimum)})
    return cases


def graph_span_cases(m):
    cases = []
    for name, points in GRAPH_SERIES.items():
        for now, window, minimum in [(100, 600, None), (100, 30, None), (100, 600, 120),
                                     (1000, 600, None), (1000, 600, 5)]:
            args = [now, window] + ([] if minimum is None else [minimum])
            cases.append({"series": name, "points": [list(p) for p in points], "now": now,
                          "window": window, "minimum": minimum,
                          "span": m.graph_span(points, *args)})
    return cases


def graph_cases(m):
    return {"segments": graph_segments_cases(m), "nice_max": nice_max_cases(m),
            "span": graph_span_cases(m)}


def view_of(result):
    out = {"state": result["state"]}
    if result["state"] == "slow":
        return out
    for key in ("ctx", "shown", "frac", "kind", "source", "estimating", "live", "gen", "prompt",
                "eta", "busy", "model"):
        out[key] = result[key]
    out["log_found"] = bool(result.get("log_found"))
    out["alerts"] = [{"level": lv if isinstance(lv, int) else None, "message": text,
                      "critical": critical} for lv, text, critical in result["alerts"]]
    out["level"] = result["level"]
    return out


def histories_of(state):
    return {"ctx_hist": [list(p) for p in state.ctx_hist],
            "speed_hist": [list(p) for p in state.speed_hist],
            "requests": [dict(r) for r in state.requests]}


def server_state_cases(m):
    cases = []
    for scenario in state_scenarios():
        levels = scenario.get("levels", [80, 95])
        state = m.ServerState(scenario["default_ctx"], warn_levels=tuple(levels),
                              history_seconds=scenario["window"])
        steps = []
        last = len(scenario["steps"]) - 1
        for i, (now, snapshot) in enumerate(scenario["steps"]):
            step = {"now": now, "snapshot": snapshot}
            step["view"] = view_of(state.update(snapshot, now))
            if scenario["full"] or i == last:
                step.update(histories_of(state))
            steps.append(step)
        cases.append({"name": scenario["name"], "default_ctx": scenario["default_ctx"],
                      "levels": levels, "window": scenario["window"], "steps": steps})
    return cases


GENERATORS = {
    "fmt_duration": fmt_duration_cases,
    "last_tokens": last_tokens_cases,
    "graph": graph_cases,
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
        head = line({k: case[k] for k in ("name", "default_ctx", "levels", "window")})
        out.append(head[:-1] + ',"steps":[')
        for si, step in enumerate(case["steps"]):
            out.append(line(step) + ("," if si < len(case["steps"]) - 1 else ""))
        out.append("]}" + ("," if ci < len(cases) - 1 else ""))
    out.append("]")
    return "\n".join(out) + "\n"


def graph_text(cases):
    """Every case on its own line, grouped by kind."""

    def line(value):
        return json.dumps(value, separators=(",", ":"))

    out = ["{"]
    kinds = list(cases)
    for ki, kind in enumerate(kinds):
        out.append(json.dumps(kind) + ":[")
        for ci, case in enumerate(cases[kind]):
            out.append(line(case) + ("," if ci < len(cases[kind]) - 1 else ""))
        out.append("]" + ("," if ki < len(kinds) - 1 else ""))
    out.append("}")
    return "\n".join(out) + "\n"


WRITERS = {"server_state": server_state_text, "graph": graph_text}


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
