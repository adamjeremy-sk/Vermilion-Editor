#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)
# Writes the expected values for the ctxmon tests by running the frozen Python original.
"""Usage: python3 tools/ctxmon_oracle.py  (rewrites tests/ctxmon/fixtures/expected/)"""
import importlib.util
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ORIGINAL = ROOT / "reference" / "ctxmon" / "ctxmon_gtk.py"
EXPECTED = ROOT / "tests" / "ctxmon" / "fixtures" / "expected"
LOGS = ROOT / "tests" / "ctxmon" / "fixtures" / "logs"


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


GENERATORS = {"fmt_duration": fmt_duration_cases, "last_tokens": last_tokens_cases}


def main():
    m = load_original()
    EXPECTED.mkdir(parents=True, exist_ok=True)
    for name, generate in sorted(GENERATORS.items()):
        path = EXPECTED / (name + ".json")
        path.write_text(json.dumps(generate(m), indent=2) + "\n", encoding="utf-8")
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
