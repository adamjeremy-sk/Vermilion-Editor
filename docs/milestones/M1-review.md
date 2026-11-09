# M1 milestone review: Model monitor logic
**Held**: 2026-11-08, Adam and the Orchestrator. **Last M1 commit**: `616fc39` (M1.6), docs `bed4146`. **Tests**: 100, all passing.

## 1. Accomplished, against the roadmap
Everything the roadmap promised for M1 was delivered, gate by gate, each audited PASS:
- `src/ctxmon/core` (library `vermilion_ctxmon_core`, no GTK) with seven modules: `format` (`fmt_duration`), `log_tokens`, `timing`, `server_api` (`/slots`, `/props`, `/v1/models`), `server_state` (histories, live speed, warnings), `graph`, `vram` (AMD sysfs under an injected root, `nvidia-smi`). About 980 lines of source; the longest file is `server_state.cpp` at 284 lines.
- The oracle: `tools/ctxmon_oracle.py` runs the frozen Python original and writes eight expected JSON files; fixtures are logs, timing, http, and fake sysfs and `nvidia-smi` trees (104 files in M1.6).
- Tests went from 67 to 100. No GTK names in core (grep check clean).

Changes to the plan: M1.0 was added (scaffold and oracle) and M1.4 was split into a and b. Deliberate differences from the original are in `vram` (signs, underscores and counter overflow rejected; ties go by name); each has a header comment and a hand test.

## 2. Learned
### Process
1. **The oracle method worked.** Every gate matched the Python original. The Orchestrator compiled, oracle-checked and mutation-tested dictated code before issuing it, and audits compared sha256 of every dictated file.
2. **Context cost.** Whole-file dictation is expensive: M1.4b ended at 108k of 131k tokens and M1.6 nearly ran out. Adam kept the method; if a session ever runs out of context it is recorded and fixed, and only then are limits set.
3. **Long writes.** The harness's write tool failed on the 496-line oracle (JSON parse error on the long argument); six edit hunks worked. For M2 the options are oracle edits as hunks, scenario data outside the script, or oracle and C++ in separate gates.
4. **Byproducts.** Qwen ran an unrequested `python3 -m py_compile` that left `tools/__pycache__/`. Rule 8 held (reported, left in place). Decision: AGENTS.md rule 21 forbids checks that write files.
5. **Faithful port versus sensible change.** Where the port deviates, the deviation is stated in a header comment and covered by a hand test.
6. **A failed continuation costs more than a fresh prompt** when the context is nearly full.
7. **How the Orchestrator asks (Adam, 2026-11-08).** Adam wants to discuss engineering options and not be excluded. The Orchestrator explains each option in plain words with a recommendation and they decide together; "you decide" is Adam's choice per item (AI_ONBOARDING.md 2.9).

### Technical
1. `std::filesystem` with `error_code`, `std::from_chars` and `g_ascii_formatd` (locale-independent) work under `-fno-exceptions`.
2. Directory order cannot be tested, so the tie sort in `vram` is only partly proven.
3. One shell function per fixture tree kept 104 files reviewable.

## 3. Parser security check (pinned for this review)
A read of the code, not a hostile-input run.
- `log_tokens`, `timing`: low risk. Counters go through `from_chars` (overflow is skipped); the patterns have no nested quantifiers; callers bound the input with `tail_start` (512 KiB), but the functions themselves accept any size.
- `server_api`: strict type checks; out-of-range integers read as absent. Unproven: json-glib on very deep nesting, and no size cap on the body.
- `vram`: `read_file` has no size limit and no regular-file check; a symlink to `/dev/zero` or a FIFO could exhaust memory or block. The clearest finding.
- `server_state`: a negative `n_ctx` or `n_decoded` gives a negative fraction and possibly a negative time-to-full. The live-speed division was checked afterwards: `live_speed` already returns no speed for equal or backwards timestamps and for a count that does not grow, so only tests are added there.
Result: gate **M1.7** (below). Regexes are recompiled on every call; that is efficiency, left for M2.8.

## 4. Decisions
- **M1.7 hardening gate before M2 (Adam), all five topics discussed and decided with him.** `vram` reads only ordinary files, at most 64 bytes, anything else is an unreadable card; `parse_slots` rejects a negative `n_ctx` or `n_decoded` where the answer is read; JSON bodies over 1 MiB are rejected in core, and a depth check before parsing (nesting over 32 levels, brackets inside strings ignored, escapes handled) rejects deeper text, because json-glib 1.8.0 on the target has no depth guard; the time-to-full estimate is removed (`eta`, its alert, `kEtaWarnSeconds`, `eta_warned_`, the oracle's `eta` field and the two scenarios `time_to_full_alert` and `time_to_full_after_down`); three hand tests lock in `live_speed` (equal timestamps, clock going backwards, count not growing; the code already returns no speed there); hostile-input hand tests for every parser (1 MB lines, deep nesting, huge numbers); the log parsers keep the 512 KiB tail and get a large-input test; the oracle is edited only to drop `eta` and the two scenarios. The first real compile with json-glib is Adam's build.
- **Time-to-full estimate removed (Adam).** It jumped by minutes because tokens per second fluctuates. Removed from the core in M1.7 (`eta`, its alert, `kEtaWarnSeconds`, `eta_warned_`, two oracle scenarios); the 80% and 95% alerts stay; no "Time until full" tile in M2.9. Negative server numbers are rejected where the answer is read (topic 2 of the M1.7 discussion).
- **AGENTS.md rule 21 (Adam).** No checks that write files (for example `py_compile`); also AI_ONBOARDING.md 2.8.
- **M2 plan unchanged (Adam).** No split; M2 starts after M1.7. UI specifications are discussed before M2.1 and M2.9.
- **Oracle lifetime (Adam asked, confirmed).** The oracle exists only for the port; it stays until M2.11 and is the side-by-side reference for the cards in M2.9. M1.7 tests do not use it.
- Open for later: `__pycache__/` in `.gitignore`; M2.8 (sysfs reads on the main loop or async, all-or-nothing `nvidia-smi` parse).

## 5. Next milestone, M2
Editor window, tabs, files, search, settings (M2.0 to M2.6), then ctxmon settings, I/O, cards, the standalone `vermilion-ctxmon` and removal of all Python (M2.7 to M2.11). Risks: the first UI work (specifications first), GtkSourceView on large files (M2.0 spike), async I/O design (M2.8), matching the Python behaviour in the cards (Adam checks).
