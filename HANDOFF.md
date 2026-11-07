# HANDOFF.md — Orchestrator Continuity Log
Maintained by the Orchestrator per AI_ONBOARDING.md §6. A new instance reads this after AI_ONBOARDING.md and ARCHITECTURE.md. The decision log of the abandoned Crimson port is archived in `docs/archive/crimson-port-decision-log.md`.

## 1. Current State  *(overwrite every update)*
| Field | Value |
|---|---|
| Updated | 2026-11-06 ~21:30 (M0.5d1 PASS after fix 1; gate commit, docs commit and push pending; M0.5d2 prompt ready; Adam approved a clang-format check, so M0.5d3 format pass is added before M0.5c) |
| FIRST ACTION on every new chat | **HANDOFF.md and AI_ONBOARDING.md live in the claude.ai Project (read them with the Projects tool, `claude/HANDOFF.md`), not on disk: a fresh chat's working directory is empty, so do not look for them there. Once they are read, and before any work on the repo, get access to the repo folder.** Call `get_device_info` first; if `connectedFolders` already lists `/home/adam/Projects/vermilion-editor` (it did on 2026-11-06), no request is needed. Otherwise call `device_request_folder_access` for that path once (the grant is recursive). If it is denied or unanswered, do not repeat it: say so, and either ask Adam to attach the folder or fall back to gated Qwen prompts. Then work through `device_bash` (the folder is mounted at `$HOME/mnt/vermilion-editor`; read-only git with `git --no-optional-locks`). Note: `device_bash` runs in a VM on Adam's machine, so tool versions seen there (python, awk) are not necessarily Adam's host versions. Editing rights are unchanged: root orchestration docs and `docs/` only, never code. The model-monitor original is in the repo at `reference/ctxmon/`. |
| Project | Vermilion Editor: native Linux text and code editor, C++17, GTK 4, GtkSourceView 5, built-in AI agent, build/debug/terminal, GPL-3.0-or-later. Written from scratch; not based on CrimsonEditor. |
| Repo state | Verified 2026-11-06 ~21:20: HEAD `e17e47b` (docs: no carried debt rule, fix loops, M0.5d1 ready), `e74ec72` (M0.5b2). Uncommitted: the 9 M0.5d1 files (`src/ui/gtkx/object_ptr.h`, `signal.h`, `signal.cpp`, `string.h`, `string.cpp`, `src/ui/main.cpp`, `tests/gtkx_object_ptr_test.cpp`, `tests/gtkx_signal_test.cpp`, `tests/gtkx_string_test.cpp`) and the Orchestrator docs edits (`HANDOFF.md`, `docs/roadmap.md`). |
| Version / HEAD | `e17e47b` plus the M0.5d1 gate commit and the docs commit; read `git log --oneline -3`. Code: `vermilion_core` (`version`, `result`, `redact`, `log`), `vermilion_agent`, `vermilion_gtkx`, executable `vermilion`, tests `vermilion_tests` and `vermilion_gtkx_tests` (58 tests). |
| Tests | 58 (2 core/agent, 9 CoreResult, 15 CoreRedact, 5 CoreLog, 11 GtkxObjectPtr, 4 GtkxString, 12 GtkxSignal), raw ctest output seen in the M0.5d1 fix 1 report. Orchestrator verified fresh 2026-11-06: exactly the 9 files changed, purpose comments on line 3, `detail` namespace, only `g_signal_connect_data` left in `src`, no line over 100 columns in `src` or `tests`. Adam confirmed the window still opens. The Orchestrator does not build. |
| Active task | Roadmap M0 debt clearing and checks: M0.5d1 done (awaiting commit). Next, in this order: M0.5d2 (core and agent debt and purpose comments; 65 tests), M0.5d3 (one-time `clang-format -i` pass), M0.5c (check script plus ctests `SourceCheck.Repo` and `Format.Check`), then the M0 documentation sweep and M1. |
| Gate position | M0.5d1 PASS (fix 1: four lines over 100 columns, caused by the prompt, rewrapped), not yet committed. M0.5d2 prompt written (verbatim below), not issued. |
| Awaiting | Adam: (1) gate commit `git add src/ui/gtkx/object_ptr.h src/ui/gtkx/signal.h src/ui/gtkx/signal.cpp src/ui/gtkx/string.h src/ui/gtkx/string.cpp src/ui/main.cpp tests/gtkx_object_ptr_test.cpp tests/gtkx_signal_test.cpp tests/gtkx_string_test.cpp && git commit -m "M0.5d1: gtkx debt cleared, main.cpp on gtkx::connect, purpose comments"`; (2) docs commit `git add HANDOFF.md docs/roadmap.md && git commit -m "docs: handoff after M0.5d1, clang-format gate, M0.5d2 ready"`; (3) `git status --short` empty, then `git push origin main`; (4) replace `<SHORT_HEAD>` in the M0.5d2 prompt, paste it into a fresh Qwen session; (5) paste Qwen's report. |
| Last prompt issued | **M0.5d2** written, NOT yet issued to Qwen. Verbatim text below, followed by the stored M0.5c prompt (issue after M0.5d3; before issuing, add the `Format.Check` ctest and the clang-format version found at M0.5d3). |
| Next action | Audit the M0.5d2 report fresh: git status (exactly the 13 files), full `git diff`, line 3 of each file, `<utility>` in `result.h`, the three-line stop expression in `redact.cpp`, no line over 100, raw ctest 65 of 65. Fix loops until no note is open, then commit and push. Then write M0.5d3: `clang-format --version` first, then `clang-format -i` on every `.cpp` and `.h` under `src` and `tests`, evidence `git diff --stat`, `clang-format --dry-run --Werror` (empty), build and full ctest; the Orchestrator reads the whole diff (formatting only, no token changes). |
| Agent / runtime | Qwen3.8-27B (local llama.cpp, `qwen-server`, port 8080) driven by OpenCode 1.19.5 over ACP inside CLion's Air plugin. Qwen is the only coding agent. Gemma permanently banned. Fresh agent session per prompt; commit only at task end (gate end). Adam builds and tests. |

**Open notes (no carried debt, Adam 2026-11-06; each is scheduled in a gate)**: M0.5d2: (g) `result.h` lacks `#include <utility>`; (h) `value() &` and `value() const&` abort paths not death-tested; (i) `redact.cpp` rule A does not stop at `\v`/`\f`; (j) untested redact paths: single-quoted values, tab separators, `authorization` ending at a comma, a name at the very end of the input; (k) purpose comments missing in `src/core`, `src/agent` and their tests. M0.5d3: code not yet checked against `.clang-format` (clang-format is not installed in the Orchestrator's VM, so only Qwen can run it). Accepted design, not debt: `value_or` needs copyable `T`; `key=` query parameters are deliberately not masked; rule A's authorization branch stops only at quotes, comma, CR and LF (values like `Basic xyz` contain spaces). Standing process facts: GLib 2.80 `g_object_add_weak_pointer` does not set the pointer on add (tests preset it); reports show raw failing build output first; `git diff` shows nothing for untracked files, so prompts ask for new files in full; prompts must keep every line they dictate within 100 columns (M0.5d1 fix 1); Qwen once replaced raw output with a summary table (M0.5d1), so prompts say "one raw block per command, never a table"; do not run dependent Orchestrator tool calls in parallel (a doc edit and its copy-back raced twice on 2026-11-06). Product notes: dogfooding markers in `docs/roadmap.md` (first at M4.3); CTXMon is a tab plus standalone `vermilion-ctxmon` (names proposed); GTK 4 has no tray icon.
**Open decisions (Adam's call)**: none block M0. Names of config files and JSON schemas are fixed when first written (ARCHITECTURE.md section 10); gdb DAP reliability is checked at M6.2; packaging at M8.

## Last prompt (verbatim)  *(M0.5d2, written 2026-11-06 evening, not yet executed; replace `<SHORT_HEAD>` with `git rev-parse --short HEAD` after the M0.5d1 and docs commits)*
````
FIRST: read AGENTS.md in the project root before doing anything else. Its rules are binding for this entire task and override anything below if they conflict. If you cannot read it, say so and STOP.
Last verified: M0.5d1 PASS, committed and pushed; tree clean at <SHORT_HEAD>

CONTEXT (BINDING)
Gate M0.5d2 of docs/roadmap.md: clear the known debt in src/core, src/agent and their tests (AGENTS.md rule 17). No new features. Every line you write stays within 100 columns (.clang-format ColumnLimit).
Files (EDIT only these 13): src/core/version.h, src/core/version.cpp, src/core/result.h, src/core/result.cpp, src/core/redact.cpp, src/core/log.cpp, src/agent/agent_info.h, src/agent/agent_info.cpp, tests/core_version_test.cpp, tests/agent_info_test.cpp, tests/result_test.cpp, tests/redact_test.cpp, tests/log_test.cpp. No CMake changes.

A. Purpose comments (AGENTS.md rule 16). Insert exactly this line as the new line 3 of each file, directly after the copyright line; change nothing else around it:
- src/core/version.h: // Version string of Vermilion Editor.
- src/core/version.cpp: // Implementation of the version string.
- src/core/result.h: // Error, Unit, Result<T> and Status: error handling without exceptions.
- src/core/result.cpp: // Explicit instantiations of Result<T>, so the template is compiled with the core flags.
- src/core/log.cpp: // Implementation of the logging layer: redact the message, then pass it to g_log as "%s".
- src/agent/agent_info.h: // Identity of the built-in agent (its name).
- src/agent/agent_info.cpp: // Implementation of the agent identity.
- tests/core_version_test.cpp: // Tests for the core version string.
- tests/agent_info_test.cpp: // Tests for the agent identity.
- tests/result_test.cpp: // Tests for Result<T> and Status, including death tests for misuse.
- tests/redact_test.cpp: // Tests for redact_secrets: rules A (named fields), B (bearer tokens) and C (key prefixes).
- tests/log_test.cpp: // Tests for the logging layer: domain, level mapping, redaction and format safety.
(src/core/redact.cpp already has one; do not change it.)

B. src/core/result.h: add #include <utility> between #include <type_traits> and #include <variant> (std::move and std::in_place_index come from it).

C. tests/result_test.cpp: add two death tests (suite CoreResult), written like the existing ValueOnErrorAborts (same ::testing::FLAGS_gtest_death_test_style = "threadsafe"; line, same EXPECT_DEATH message regex "Result::value\\(\\) called on an error"):
1. ValueLvalueOnErrorAborts: Result<int> r = Result<int>::err(Error{"e"}); EXPECT_DEATH(r.value(), ...);
2. ValueConstOnErrorAborts: const Result<int> r = Result<int>::err(Error{"e"}); EXPECT_DEATH(r.value(), ...);
The file already has using vermilion::Error; and using vermilion::Result;.

D. src/core/redact.cpp, rule A: non-auth values must also stop at '\v' and '\f'. Replace the two lines of the non-auth stop expression (currently lines 95 and 96, the ": (c == ' ' ..." line and the "c == ',' ..." line under it) with exactly these three lines (30 and 33 spaces of indent, as the current lines have):
                              : (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
                                 c == '\r' || is_quote(c) || c == ',' || c == ';' || c == '&' ||
                                 c == '}');
The authorization branch (is_auth ? ...) is unchanged.

E. tests/redact_test.cpp: add five tests (suite CoreRedact); compare std::string values with EXPECT_EQ as the file does:
1. VerticalTabAndFormFeedEndValue: "token=abc\vnext" gives "token=[REDACTED]\vnext"; "secret=xyz\fmore" gives "secret=[REDACTED]\fmore".
2. SingleQuotedValue: "password='hunter2' ok" gives "password='[REDACTED]' ok"; "'token': 'abc'" gives "'token': '[REDACTED]'".
3. TabSeparators: "token\t=\tabc123\tnext" gives "token\t=\t[REDACTED]\tnext".
4. AuthorizationEndsAtComma: "Authorization: Basic abc, next" gives "Authorization: [REDACTED], next".
5. NameAtEndOfInputUnchanged: "token", "my password" and "Authorization:" each come back unchanged.
If any of these expectations fails, paste the raw failure and STOP; do not change redact.cpp beyond item D to make a test pass.

STRICT RAILS
1. ONE step only. Do not commit, push or stage anything. Do not run the GUI program.
2. Touch only the 13 files named above.
3. Paste raw command output in fenced blocks, unaltered, one block per command. Never use "..." inside a raw block, never replace raw output with a summary or a table, never retype output by hand. If you trim, cut only from the end and write "[N lines trimmed]". An empty output is an empty raw block.
4. Every modified file is shown as raw git diff output (all 13 are tracked).
5. No C++ exceptions, no new dependencies, no invented functions. If a build or test fails because of your own code, paste the raw error lines FIRST, then fix it within the listed files and show each fix. If something in this prompt does not compile as written, say so and show the minimal fix.

STEPS
STEP 1 (the only step):
1. Run: git --no-optional-locks status --short. It must print nothing. If it prints anything, STOP and report it.
2. Make the changes A to E.
3. Run: cmake --build build 2>&1 | tail -n 15; echo "exit=$?" then ctest --test-dir build --output-on-failure; echo "exit=$?" (in full). Expected 65 tests: 58 existing plus 2 CoreResult plus 5 CoreRedact.
4. Proofs (raw): for f in src/core/version.h src/core/version.cpp src/core/result.h src/core/result.cpp src/core/redact.cpp src/core/log.cpp src/agent/agent_info.h src/agent/agent_info.cpp tests/core_version_test.cpp tests/agent_info_test.cpp tests/result_test.cpp tests/redact_test.cpp tests/log_test.cpp; do printf '%s: ' "$f"; sed -n 3p "$f"; done; grep -n "#include" src/core/result.h; awk 'length>100 {print FILENAME":"FNR": "length}' src/core/*.h src/core/*.cpp src/agent/*.h src/agent/*.cpp tests/*.cpp; echo "exit=$?" (expected no lines); wc -l src/core/version.h src/core/version.cpp src/core/result.h src/core/result.cpp src/core/redact.cpp src/core/log.cpp src/agent/agent_info.h src/agent/agent_info.cpp tests/core_version_test.cpp tests/agent_info_test.cpp tests/result_test.cpp tests/redact_test.cpp tests/log_test.cpp.
5. Run: git --no-optional-locks status --short (raw; expected exactly the 13 files as M), then git --no-optional-locks diff (raw, all 13 files).
EVIDENCE: raw blocks for items 1 to 5 and the test count from ctest.

End your response with the exact phrase "STOPPING AND WAITING FOR VERIFICATION" and cease all generation.
````

## Stored prompt: M0.5c  *(issue after M0.5d3 is committed; first add the `Format.Check` ctest (`clang-format --dry-run --Werror` on the same file set) and change its Last verified line to M0.5d3; replace `<SHORT_HEAD>` then)*
````
FIRST: read AGENTS.md in the project root before doing anything else. Its rules are binding for this entire task and override anything below if they conflict. If you cannot read it, say so and STOP.
Last verified: M0.5d3 PASS, committed and pushed; tree clean at <SHORT_HEAD>

CONTEXT (BINDING)
Gate M0.5c of docs/roadmap.md, the last gate of M0: a check script that enforces AGENTS.md rule 5 (SPDX and copyright header on every source file) and the purpose comment of rule 16 (the line directly after the copyright line is a comment), and reports file sizes against rule 16. It is registered as a ctest test so every build run checks the tree.
Files: NEW tools/check_sources.sh (make it executable with chmod +x). EDIT tests/CMakeLists.txt: append exactly one line at the end of the file: add_test(NAME SourceCheck.Repo COMMAND ${CMAKE_SOURCE_DIR}/tools/check_sources.sh). Do not touch any other file. Do not add or change any C++ code.

SCRIPT (binding). Bash only, using only bash, find, sort, awk and printf. No Python, no temporary files, no network.
Header: line 1 #!/usr/bin/env bash, line 2 # SPDX-License-Identifier: GPL-3.0-or-later, line 3 # Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk), line 4 a one-sentence comment saying what the script is for. Then set -u -o pipefail (NOT set -e, because a non-matching check is normal).
Inputs:
- No arguments: cd to the repository root with cd "$(dirname "$0")/.." and check every file found by find src tests tools -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.sh' \), sorted with LC_ALL=C sort. Nothing else is scanned (reference/, res/, docs/, build/ and cmake-build-*/ are never checked).
- One or more arguments: check exactly those paths, in the given order, relative to the caller's current directory (do not cd). A path that is not a readable file prints MISSING-FILE <path> and makes the script exit 2 at the end.
Per file, read the file exactly ONCE with this awk program (the input may be a pipe, so a second read would see nothing):
awk 'NR<=5 && /SPDX-License-Identifier: GPL-3\.0-or-later/ {s=1} NR<=5 && /Copyright \(C\) [0-9][0-9][0-9][0-9].* Adam Hainstock \(@adamjeremy-sk\)/ {c=1; cl=NR; next} cl && NR==cl+1 && /^(\/\/|#) [^ ]/ {p=1} END {print NR, s+0, c+0, p+0}' "$f"
(the fourth field is 1 when the line directly after the copyright line is a // or # comment)
Limits: a path that starts with tests/ uses review 500 and fail 800; every other path uses review 300 and fail 500. Status: lines > fail is FAIL; else lines > review is REVIEW; else OK.
Output, one line per file: printf '%-6s %5d %s\n' "$status" "$lines" "$f". If the SPDX line or the copyright line is missing in the first 5 lines, also print MISSING-HEADER <path>. If the purpose comment is missing, also print MISSING-PURPOSE <path>.
Last line: checked N files: H missing header, P missing purpose, R review, F fail, M missing file (the five counts).
Exit code: 2 if any MISSING-FILE; else 1 if any MISSING-HEADER, MISSING-PURPOSE or FAIL; else 0. REVIEW alone never fails.

STRICT RAILS
1. ONE step only. Do not commit, push or stage anything. Do not run the GUI program.
2. Touch only the two files named above. No docs, LICENSE, opencode.json, res/, reference/.
3. Paste raw command output in fenced blocks, unaltered. Never use "..." inside a raw block, never summarize in place of raw output, never retype output by hand. If you trim, cut only from the end and write "[N lines trimmed]". An empty output is an empty raw block.
4. Show the new file in full and the modified file as the raw git diff output.
5. No new dependencies, no invented commands or options. If a build or test fails because of your own code, paste the raw error lines FIRST, then fix it within the listed files and show each fix. If something in this prompt does not work as written (for example the awk program), say so, show the raw error and the minimal fix.

STEPS
STEP 1 (the only step):
1. Run: git --no-optional-locks status --short. It must print nothing. If it prints anything, STOP and report it.
2. Make the changes in CONTEXT, including chmod +x tools/check_sources.sh.
3. Run (raw, each followed by echo "exit=$?"):
   a. tools/check_sources.sh (full output; expected every file OK, exit 0)
   b. tools/check_sources.sh <(printf 'int x;\n') (expected OK, 1 line, MISSING-HEADER and MISSING-PURPOSE, exit 1)
   c. tools/check_sources.sh <(printf '// SPDX-License-Identifier: GPL-3.0-or-later\n// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)\n// Test input.\n'; yes '' | head -n 298) (expected REVIEW, 301 lines, exit 0)
   d. tools/check_sources.sh <(printf '// SPDX-License-Identifier: GPL-3.0-or-later\n// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)\n// Test input.\n'; yes '' | head -n 498) (expected FAIL, 501 lines, exit 1)
   e. tools/check_sources.sh no/such/file.cpp (expected MISSING-FILE, exit 2)
4. Run: cmake -S . -B build (last 10 lines), cmake --build build (last 15 lines), ctest --test-dir build --output-on-failure (in full). Expected 55 tests: 54 existing plus SourceCheck.Repo.
5. Proofs (raw): ls -l tools/check_sources.sh (must show the x bits); head -n 4 tools/check_sources.sh; grep -nE "mktemp|/tmp|python|curl|wget" tools/check_sources.sh (expected empty); wc -l tools/check_sources.sh tests/CMakeLists.txt.
6. Run: git --no-optional-locks status --short (raw; expected exactly ?? tools/ and M tests/CMakeLists.txt), then git --no-optional-locks diff -- tests/CMakeLists.txt (raw).
EVIDENCE: raw blocks for items 1 to 6, the new file in full, and the test count from ctest.

End your response with the exact phrase "STOPPING AND WAITING FOR VERIFICATION" and cease all generation.
````

## 2. Decision Log  *(append only, one line each, newest last)*
- 2026-11-05 — (Adam) **Reset.** Crimson Editor is abandoned as the base: the project is rebuilt from scratch. This supersedes the earlier decisions to hard-fork CrimsonEditor, the MIT licence, the custom editing widget, the `.spc/.key` syntax format, the Spencer regex engine, the macro/user-tool/column-mode features and the phase A to H removal roadmap. The history of that port is in `docs/archive/` and in git.
- 2026-11-05 — (Adam) **Scope trimmed** to: a directory sidebar on the right-hand side (specification kept in `docs/requirements/directory-sidebar.md`), an integrated agent (OpenAI standard, Anthropic and Gemini supported), the ctxmon model monitor ported from `~/Projects/llm-suite-test/ctxmon_gtk.py`, building code in the application and debugging it. Added: a terminal (VTE, also in the monitor), a project file, slash commands with a gate framework, MCP, cost display. Dropped from the first scope: git integration, multi-cursor, plugin hook, theme system, SSH, session restore, indexed project search.
- 2026-11-05 — (Adam) **Language C++17** (after weighing C): RAII and `std::string` for the logic-heavy modules; GTK 4 and GLib through their plain C APIs. **No C++ exceptions in project code** (GTK callbacks are C). **No wrapper rule**: nothing that exists only to wrap another API; libraries that provide a widget or service (GtkSourceView, VTE, libsoup, json-glib, libsecret) are fine.
- 2026-11-05 — (Adam) **Licence GPL-3.0-or-later**, copyright "Adam Hainstock (@adamjeremy-sk)", SPDX header on every source file. Supersedes MIT. The licence text is taken from the GPL-3.0 text in `llm-suite-test` (Adam's own repository, which holds ctxmon).
- 2026-11-05 — (Adam) **Editor widget: GtkSourceView 5.** Target system Ubuntu 24.04.5 (gdb 15.1; GTK 4.14 expected). Development packages to install: `libgtk-4-dev libgtksourceview-5-dev libjson-glib-dev libsoup-3.0-dev libsecret-1-dev libvte-2.91-gtk4-dev libgtest-dev cmake pkg-config clang-format clang-tidy build-essential`.
- 2026-11-05 — (Adam) **Agent**: own agent loop with provider adapters (OpenAI-compatible first, which also reaches Gemini's compatible endpoint; native Anthropic; native Gemini only if needed); streaming; direct model access with user keys in the keyring; tools run_command and the seven others. **Permission presets** Restricted (default), Balanced (Adam's preference, like the coding session), Unrestricted, and Custom JSON. Unrestricted needs a confirmation dialog and a persistent red badge; in every preset the agent cannot change its own preset and cannot read API keys. Setup dialog on first use of the Agent tab. `/clear` archives the chat; `/history`, `/copy`; Copy buttons everywhere; slash commands as Markdown templates, with the gate framework (`/gate`, `/audit`) as built-in ones. Cost display with user-defined input and output prices (including for a local model). Protocols: native loop and MCP (server and client over stdio); ACP deferred. Context, streaming, keys, review, undo, privacy and limits as in ARCHITECTURE.md section 7.6.
- 2026-11-05 — (Adam) **Review and external changes**: side-by-side diff (option B) with folding and per-hunk Accept/Revert; external changes reload a clean buffer automatically and show a banner for a dirty one.
- 2026-11-05 — (Adam) **Debugger**: Debug Adapter Protocol, gdb's built-in DAP first (gdb 15.1), GDB/MI as the fallback if the M6.2 spike shows DAP unreliable. **Build**: per-folder configuration in `.vermilion/project.json` with presets for CMake, Make, Ninja, Cargo, Gradle and Bazel, problem matchers for gcc, clang, rustc, javac.
- 2026-11-05 — (Adam) **Project file** (`PROJECT.md` plus `.vermilion/project.json`; names proposed) tracks the project layout and decisions as the user builds; created by New Project and by "Adopt as project".
- 2026-11-05 — (Adam) **Order and versions**: v0.1 editor, sidebar, build/Output/Problems, terminal; v0.2 agent core and diff; v0.3 debugger; v0.4 monitor panel, cost, Anthropic/Gemini, project file, gate framework, MCP. ctxmon logic (M1) is built early as the first job for Qwen, with tests derived from the Python original. Application ID `io.github.adamjeremy_sk.Vermilion`.
- 2026-11-05 — **Reset kept files** (Orchestrator proposal, Adam confirmed "kept = icons plus process docs"): `res/vedt/`, `LICENSE`, `opencode.json`, `AGENTS.md`, `AI_ONBOARDING.md`, `ARCHITECTURE.md`, `HANDOFF.md`, `README.md`, `docs/roadmap.md`, `docs/requirements/`, `docs/archive/`, `.gitignore`. Everything else, including `docs/legacy/`, is deleted by M0.1. The claude.ai Roadmap Doc is obsolete; `docs/roadmap.md` is the roadmap.

- 2026-11-05 — M0.1 (reset, commit `67e7e70 (was 39d9016 before the rewrite)`) done by Adam: 14 tracked files remain, `upstream` removed. M0.2 (CMake skeleton and empty window) executed by Qwen and audited PASS WITH NOTES. Orchestrator re-check with fresh commands: only the expected files, SPDX header present in all 7 sources, no GTK names in `src/core` or `src/agent`, GTK 4 APIs only, ignore rules cover `build`, `cmake-build-*`, `.idea`, `skills`. Qwen found that the two tests failed because `EXPECT_EQ` compared `const char*` pointers (the Orchestrator's prompt said "equals" without naming the comparison); fixed by comparing `std::string`. Notes: (1) `vermilion_core` links `vermilion_warnings` and `vermilion_noexc` as PUBLIC, so `-fno-exceptions` also reaches `vermilion_agent`, the UI and the tests (compile commands show it on all 5 files); the spec said the tests must not get it. Fixed in M0.3. (2) The window has not been opened by the Orchestrator or by Qwen (no display); Adam checks it. Qwen found GTK 4.14.5, GtkSourceView 5.12.0, json-glib 1.8.0, libsoup 3.4.4, libsecret 0.21.4, VTE 0.76.0, GoogleTest 1.14.0.
- 2026-11-05 — M0.3 (linkage fix, `gtkx` `ObjectPtr` and string helpers, 12 tests) executed by Qwen, audited PASS WITH NOTES (see section 1 carry-forward notes). Orchestrator fresh check: `-fno-exceptions` present on the 4 non-test sources and absent on the 4 test sources, `-Wall` and `-Werror` on all 8, SPDX present in all .cpp/.h, no GTK names in `src/core` or `src/agent`, no GTK 3 calls.
- 2026-11-05 — (Adam) **History rewrite**: drop all history before the M0.1 reset commit (`39d9016`), which becomes the root; replay later commits; one force-push to `origin main` after the M0.3 commit; backup tag kept locally. Reason: the Crimson code lives in older history. Old objects may stay reachable on GitHub by hash until GitHub purges them. Done 2026-11-05: new root `67e7e70`, force-pushed as `13ee5bc`; trees identical to the backup (`git diff --stat` empty).
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
- The Project holds `claude/GATE0_ANALYSIS.md`, a historical analysis of the abandoned port; it is not binding.
- Dropped and not scheduled: git integration, multi-cursor, theme system, SSH, session restore, indexed search, plugin hook.

## 4. Session Log  *(newest first, keep 10)*
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
### 2026-11-05 (evening, late) — M0.4 audit, size rules
- History rewrite done (root `67e7e70`, force-pushed). M0.4 audited PASS WITH NOTES (25 tests). Adam asked about keeping files modular; rule 16 written (limits 300/500, tests and data 500/800). M0.5 split; M0.5a prompt issued. Open: commits and push, tag cleanup check, M0.5a run and audit.
### 2026-11-05 (evening, later) — M0.3 audit and history rewrite decision
- M0.2 window confirmed open. M0.3 report audited PASS WITH NOTES (12 new tests, 14 total; flags, SPDX, layer and GTK 3 proofs checked fresh). Adam decided to rewrite history with `39d9016` as the new root, after the M0.3 commit, with one force-push. M0.4 prompt issued. Open: M0.3 commit, docs commit, rewrite and push, M0.4 run and audit.
### 2026-11-05 (evening, end) — M0.2 and handoff
- M0.1 reset done by Adam (`67e7e70 (was 39d9016 before the rewrite)`); M0.2 (CMake skeleton, empty window, 2 tests) executed by Qwen, audited PASS WITH NOTES, committed and pushed by Adam (`5d21e3c` (was ac38710)). The M0.3 prompt (linkage fix plus `gtkx` ObjectPtr and string helpers) was issued; Adam ended the session before running it. Open: docs commit, M0.3 run and audit, then M0.4.
### 2026-11-05 (evening) — Reset to a greenfield plan
- Adam abandoned Crimson as the base and cut the scope to the sidebar, the agent, the ctxmon port and build/debug; the Orchestrator read `ctxmon_gtk.py` (1,587 lines, GTK 3, Adam's own GPL-3 code), weighed C versus C++ (C++17 chosen), confirmed the target system (Ubuntu 24.04.5, gdb 15.1) and settled the licence (GPL-3.0-or-later). All decisions are in the Decision Log; ARCHITECTURE.md, docs/roadmap.md, AGENTS.md, AI_ONBOARDING.md, README.md and LICENSE were rewritten; the old decision log, the sidebar requirement and the archive were saved under `docs/`. The Crimson port had reached C6 (see `docs/archive/`). Open: Adam commits the docs, installs packages, runs M0.1.
