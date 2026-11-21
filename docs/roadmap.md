# Roadmap: Vermilion Editor

**Status**: rewritten 2026-11-05 for the greenfield plan. This is the live roadmap (the old claude.ai Roadmap Doc is obsolete). Architecture: ARCHITECTURE.md. Process: AI_ONBOARDING.md and AGENTS.md.

**Line rule**: no line of this file has more than 60 words. A gate's table row stays short. Longer detail and the notes go in `docs/gate-details.md`; a row that says "see gate-details" has more there. Check: `awk 'NF > 60' docs/roadmap.md` prints nothing.

## How this plan works

- **Gates.** Each gate is one step, with evidence and an audit verdict. A gate that adds code also adds its tests. Adam builds, tests and commits; the work is pushed at the end of each gate.
- **Milestone reviews.** Every milestone ends with a review with Adam before the next one starts (AI_ONBOARDING.md section 2, item 7). Its conclusions go to `docs/milestones/M<n>-review.md`.
- **The Done column.** ☐ means not done. ☑ means done, followed by the date (YYYY/MM/DD). A note mark (\*, †, ‡, ¶, §, ‖, \#, then doubled: \*\*, ††, ...) points to the notes of that milestone in `docs/gate-details.md`; the marks start again in each milestone. Each milestone also has a Status line.
- **Standing objective: REUSE certification.** The repository is to pass `reuse lint` (https://reuse.software), with the licence texts in `LICENSES/`. Everything created from 2026-11-17 on is compliant when it is made (AGENTS.md rule 5); older files are brought into line at M9. Already done: the vendored xxHash (`third_party/xxhash/`, with a `.license` sidecar) and `LICENSES/`.

## Progress

- **Counting.** Every milestone has a fixed share of the 100%. A milestone's share is split equally among its own gates, so adding or splitting a gate changes only that milestone's fraction, never the total. M9 counts as one gate.
- **Formula.** Progress is the sum, over all milestones, of weight × (gates ticked ☑ ÷ gates in the milestone). The bar has 20 cells of 5% each; the bar and the percentage are rounded down.
- **Upkeep.** The weights are the Orchestrator's estimate, accepted by Adam; they change only when Adam decides. The bar is updated with the Done column at every gate commit, and the same line is in README.md.
- **Weights** (sum 100): M0 5 · M1 6 · M2 19 · M3 8 · M4 6 · M5 25 · M6 10 · M7 10 · M8 5 · M9 6.
- **Share of one gate** in its milestone: M0 0.42% (12 gates) · M1 0.55% (11) · M2 1.19% (16) · M3 1.33% (6) · M4 1.20% (5) · M5 1.67% (15) · M6 1.25% (8) · M7 1.67% (6) · M8 1.67% (3) · M9 6.00% (1).
- **Now**: 5 + 6 + 19 × 11/16 = 24.1, shown as 24%.

**Progress**: `████░░░░░░░░░░░░░░░░` 24% (weighted: M0 and M1 done; M2 11 of 16)

## Versions
| Version | Contents | Milestones |
|---|---|---|
| 0.1.0 | The light editor `vermilion` and the IDE `vermilion-ide` with the editor, search, settings, crash recovery, the diff view and the right-hand directory sidebar; the ctxmon logic is built and tested (no window yet) | M0 to M3 |
| 0.2.0 | Build and run, the Output pane, Problems; Vermilion builds itself (DOGFOOD 2 and 3) | M4 |
| 0.3.0 | Agent core, local first (llama.cpp through the OpenAI-compatible API): provider, permission engine, tools, agent loop, change sets with whole-turn and per-hunk revert, staged edits, chat, setup dialog, archive and history, request log, privacy indicator (the diff engine and view are in 0.1.0) | M5 |
| 0.4.0 | ctxmon UI: cards and graphs, the terminal, the standalone `ctxmon` and the ctxmon tab; all Python removed | M6 |
| 0.5.0 | Debugger (DAP) | M7 |
| 0.6.0 | Cost display, project file, gate framework | M8 |
| 0.9.0 | Packaging, documentation, notices | M9 |
| 1.0.0 | After 0.9.0, `1.0.0-beta.N` builds run as a beta for 8 weeks, then `1.0.0-stable` is released; from then on there are `-stable` and `-beta` branches and extra features are considered (`docs/ideas.md`) | after M9 |

- **Tags.** A version is tagged when the last milestone of its row is closed, at that milestone's review (AI_ONBOARDING.md section 2, item 7). The program's version string (`CMakeLists.txt` and `src/core/version.cpp`, still 0.0.1) is raised at each tag.
- **Dogfooding markers (◆)** are the gates where Adam starts building Vermilion with Vermilion:
  - DOGFOOD 1 at M2.7b: the light editor `vermilion` replaces gedit and Mousepad on Adam's system (set 2026-11-11).
  - DOGFOOD 2 at M4.3: self-build (set 2026-11-05).
  - DOGFOOD 3 at M4.5: Vermilion is the primary editor for this repo.
  - DOGFOOD 4 at M5.7b: the built-in agent is tried against local llama.cpp (set 2026-11-13). It has the permission engine, tools with staged edits, loop, diff engine, whole-turn revert, chat UI and pending-changes bar to work with. CLion/OpenCode stays the primary agent until Adam judges the built-in one good enough.
- **ctxmon logic (M1)** is built early because it is self-contained. The Python original (`ctxmon_gtk.py` in `reference/ctxmon/`, 1,587 lines; logo `reference/ctxmon/ctxmon.svg`) is an executable specification, the best first job to prove the workflow with the local agent. Since 2026-11-07 the port in `src/ctxmon/` is the source of truth and the Python original is frozen as the test oracle.
- **ctxmon's UI** is its own milestone, M6: settings, I/O, cards and terminal, the standalone program and the ctxmon tab (which needs the right-hand stack from M3).

## M0: Reset and foundation
**Status**: ☑ 2026/11/07 \*

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M0.1 | Reset the tree: one `git rm -r` of everything except the kept files; GPL-3 `LICENSE`; remove the `upstream` remote; commit docs | `git ls-files` shows only the kept files; tree clean | Adam, Orchestrator checks | ☑ 2026/11/05 |
| M0.2 | CMake skeleton: top level, `src/core`, `src/agent`, `src/ui`, `tests`, GoogleTest target, `.clang-format`, `.gitignore`, a `GtkApplication` window with the application ID | Configures and builds; empty window opens; `ctest` runs one trivial test | Qwen | ☑ 2026/11/05 |
| M0.3 | Fix target linkage (`-fno-exceptions` must not reach the tests); `src/ui/gtkx` library with `ObjectPtr` and string helpers, tested without a display | Compiles; `vermilion_gtkx_tests` pass; compile commands show tests without `-fno-exceptions` | Qwen | ☑ 2026/11/05 |
| M0.4 | `gtkx` signal connections by lambda (RAII disconnect) | Tests with `GListStore` and `GSimpleAction` pass | Qwen | ☑ 2026/11/05 |
| M0.5a | `Error` and `Result<T>` types in core (`Status` = `Result<Unit>`), tested incl. death tests for misuse | Tests pass; header compiles under `-fno-exceptions` | Qwen | ☑ 2026/11/05 |
| M0.5b1 | `redact_secrets` (on a `std::string_view`) in core: masks API keys, bearer tokens and secret-named fields (pure text, no GLib) | Tests pass; header and source compile under `-fno-exceptions` | Qwen | ☑ 2026/11/05 |
| M0.5b2 | Log layer over GLib `g_log` (domain `vermilion`), every message passed through `redact_secrets`; links GLib into core. A thin layer is allowed because it adds redaction (wrapper rule) | Tests capture messages with a log handler; no secrets in log output | Qwen | ☑ 2026/11/06 |
| M0.5d1 | Debt clearing (runs before M0.5c; no carried debt): `gtkx` internals into `detail`, `SignalConnection` doc comment, `ObjectPtr` assignment tests, disconnect inside a handler test, `main.cpp` on `gtkx::connect`, purpose comments in `src/ui` and the gtkx tests | Tests pass (58); window still opens (Adam) | Qwen | ☑ 2026/11/06 |
| M0.5d2 | Debt clearing (runs before M0.5c): `result.h` `<utility>` and the `&`/`const&` abort death tests; `redact.cpp` stops at `\v`/`\f` and the four untested paths; purpose comments in `src/core`, `src/agent` and their tests | Tests pass (65) | Qwen | ☑ 2026/11/06 |
| M0.5d3 | One-time `clang-format -i` pass over every `.cpp` and `.h` in `src` and `tests` with the committed `.clang-format`; no other change | `clang-format --dry-run --Werror` is clean; tests pass | Qwen | ☑ 2026/11/06 |
| M0.5c1 | Check script `tools/check_sources.sh` as ctest `SourceCheck.Repo`: SPDX header and purpose comment on every source file; file-size report (review above 300 lines, fail above 500; tests and data 500 and 800; AGENTS.md rule 16); `evidence/` in `.gitignore` | No file lacks a header or purpose comment; 66 tests pass | Qwen | ☑ 2026/11/07 |
| M0.5c2 | Format check `tools/check_format.sh` as ctest `Format.Check`: `clang-format --dry-run --Werror` on every `.cpp` and `.h` in `src` and `tests` (clang-format 18.1.3 on Adam's machine); skipped, not failed, when clang-format is missing | 67 tests pass | Qwen | ☑ 2026/11/07 |

## M1: Model monitor logic (core, tests from the Python oracle)
**Status**: ☑ 2026/11/10 \*

Revised at the M0 review (2026-11-07): pure logic only, in `src/ctxmon/core` (no GTK; GLib, `GRegex` and json-glib allowed), tests in `tests/ctxmon/`. Functions take text, JSON strings or a directory root; no HTTP, threads or polling (M6.2). Expected values come from the frozen Python original through the oracle script.

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M1.0 | Scaffold `src/ctxmon/core` (library `vermilion_ctxmon_core`) and `tests/ctxmon` (`vermilion_ctxmon_tests`); `tools/ctxmon_oracle.py` runs the Python original and writes expected values as JSON to `tests/ctxmon/fixtures/expected/`; first port `fmt_duration`. Each later gate adds its own fixtures and one oracle generator | Builds; oracle output committed; 69 tests pass | Qwen | ☑ 2026/11/07 |
| M1.1 | Token lines: `TOKEN_RE` kinds final/prompt/past, last match wins, session floor and truncation (`last_tokens` on text plus offsets) | Tests match the oracle | Qwen | ☑ 2026/11/07 |
| M1.2 | Timing lines (`GEN_RE` with the lookbehind, `PROMPT_RE`), `last_speeds`, `parse_requests` (pairing prompt, generation and the context line after) | Same | Qwen | ☑ 2026/11/07 |
| M1.3 | `/slots` and `/props` parsing on JSON strings (n_ctx, is_processing, n_decoded in both `next_token` shapes; model alias, else file name; `/v1/models` fallback) | Same, on captured JSON | Qwen | ☑ 2026/11/08 |
| M1.4a | `live_speed`, `ServerState` without warnings (histories and pruning, requests, exact versus estimated tokens); the oracle scripts the snapshot sequences | Same; 87 tests | Qwen | ☑ 2026/11/08 |
| M1.4b | `ServerState` warnings: levels 80% and 95% with hysteresis, the first-update rule, time-to-full alert; `down` resets the warning state; `ServerState::update` split into helpers | Same, alerts and level in the scripted sequences | Qwen | ☑ 2026/11/08 |
| M1.5 | Graph geometry: `graph_segments`, `nice_max`, `graph_span` | Same | Qwen | ☑ 2026/11/08 |
| M1.6 | VRAM readers: AMD sysfs under an injected root, `nvidia-smi` output parsing | Tests on fake sysfs trees and sample output | Qwen | ☑ 2026/11/08 |
| M1.7a | Split from M1.7 (gate size). Time-to-full estimate removed (`eta`, its alert, `kEtaWarnSeconds`; oracle and `server_state.json` regenerated, 20 scenarios); three `live_speed` hand tests | 103 tests passing | Qwen | ☑ 2026/11/10 † |
| M1.7b | Split again (gate size). `server_api`: bodies over 1 MiB and nesting deeper than 32 levels count as not JSON (json-glib 1.8.0 crashes on 100,000 levels); negative `n_ctx` or `n_decoded` count as missing; six hand tests | 109 tests passing | Qwen | ☑ 2026/11/10 ‡ |
| M1.7c | `vram` `read_file` reads only ordinary files of at most 64 bytes, found by reading (sysfs reports 4096 for every file); anything else counts as missing; four `vram` tests on a temporary tree; two `log_tokens` and two `timing` tests (1 MB lines, huge numbers) | 117 tests passing | Qwen | ☑ 2026/11/10 ¶ |

## M2: Editor
**Status**: ☐

**Before any UI gate:** the first gate that builds UI (M2.1, and again before the ctxmon cards M6.3) is preceded by a talk with Adam about his UI specifications; no UI prompt is written before that.
The talk for the M2 window was held 2026-11-10; its outcome is ARCHITECTURE.md section 7.10 and the HANDOFF.md decision log. The right-hand stack and sidebar (M3) and the ctxmon cards (M6.3) get their own talks.

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M2.0 | Two spikes: a 50 MB file in GtkSourceView, and the look (see gate-details) | Numbers recorded in HANDOFF.md; Adam judges the look by eye before M2.1 starts | Orchestrator plans, Qwen codes, Adam measures and looks | ☑ 2026/11/10 \* |
| M2.1 | Main window: menu bar, tabs, status bar, shared `GAction`s (see gate-details) | Opens and closes tabs; Adam checks the look (see gate-details) | Qwen (split as needed: window and actions, then tabs) | ☑ 2026/11/11 † |
| M2.1c | Application icons: both SVGs rendered to PNG at every build (see gate-details) | Icon above the name in About; small icon in the title bar | Qwen | ☑ 2026/11/11 ‡ |
| M2.1d | Continuous integration on GitHub Actions (see gate-details) | The first run on GitHub is green | Qwen | ☑ 2026/11/11 ¶ |
| M2.2 | Open and save, encodings, line endings (see gate-details) | Core tests for the scanner, formats, document list and messages; Adam's 18-point run of open and save | Qwen | ☑ 2026/11/11 § |
| M2.2b | Help → Index: the manual in a read-only window (see gate-details) | Every topic opens and reads correctly; renderer, history and manual tests | Qwen writes the renderer, the page history and the window from specs; headers, tests and wiring dictated | ☑ 2026/11/11 ‖ |
| M2.2c | Crash recovery (see gate-details) | A killed editor offers the unsaved text at the next start; core tests | Qwen | ☑ 2026/11/12 \# |
| M2.3 | Search, replace (regex), go to line; the Search menu | Manual check plus core tests for the search options and the replacement rules | Qwen | ☑ 2026/11/17 \*\* |
| M2.4 | File monitor and the external-change policy (the banner's Show diff button joins with the diff view, M2.6) | Edit on disk reloads a clean buffer; dirty buffer shows the banner | Orchestrator plans, Qwen codes | ☑ 2026/11/18 †† |
| M2.4b | Files from the command line: `vermilion file1 file2 ...`, a terminal question for missing files (create them), the open queue, the manual topic Command-line options (see gate-details) | Core tests for the argument sorting, the queue and the question machine; Adam's hands-on run from a terminal | Orchestrator plans, Qwen codes | ☑ 2026/11/19 ‡‡ |
| M2.5 | Settings window and key file: colour mode, schemes, editor font, tab width; the Tools menu (see gate-details) | Settings persist; Adam checks light and dark | Orchestrator plans, Qwen codes | ☑ 2026/11/21 ¶¶ |
| M2.5b | Help window with a contents tree and a search tab (see gate-details) | Manual check: tree, search with each option, Back, Forward, Hide; core tests for the page search and the history | Design talk with Adam, then plan first (rule 10) | ☐ |
| M2.6 | Diff engine and `DiffView` (see gate-details) | Tests incl. edge cases (applying a diff to A gives B); manual check of the view, the banner button and the Compare button | Orchestrator plans, Qwen codes | ☐ |
| M2.7 | Window size restored; start state (a new untitled document by default; or reopen the last document, a single file; or a blank window); no tab limit | Restored on start | Qwen | ☐ |
| M2.7c | File → Open Recent (proposed at the end of M2.3; design talk first, see gate-details) | Opens a recent file; a missing file is handled as the talk decides | Plan first (rule 10) | ☐ |
| M2.7b | The light program `vermilion` (ARCHITECTURE.md 7.12; see gate-details) | Opens text files from the file manager; the link check passes; Adam measures startup. **◆ DOGFOOD 1: `vermilion` replaces gedit and Mousepad on Adam's system** | Orchestrator plans, Qwen codes | ☐ |

## M3: Directory sidebar (right-hand side) (v0.1.0 with M1 and M2)
**Status**: ☐

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M3.0 | The `vermilion-ide` executable appears (ARCHITECTURE.md 7.12): a thin `main` over the editor base, application ID `io.github.adamjeremy_sk.VermilionIde`, recovery folder `ide`. Then Open Folder, Recent Folders, Close Folder and restoring the last folder, with their File menu items | Folder restored on start | Qwen | ☐ |
| M3.1 | FileOps interface in core with a fake, plus tests | Acceptance tests in the sidebar requirement (`docs/requirements/`) pass | Qwen | ☐ |
| M3.2 | Tree widget, lazy loading, folders first | Opens a folder, lists files | Qwen | ☐ |
| M3.3 | Filter, hidden-files toggle, Sync, Refresh, keys | Manual check against the requirement | Qwen | ☐ |
| M3.4 | Context menus, rename, delete (trash), copy, move, drag and drop | Same | Qwen | ☐ |
| M3.5 | Folder monitor | Tree follows external changes | Qwen | ☐ |

## M4: Build, Output, Problems (v0.2.0)
**Status**: ☐

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M4.1 | Project file model (build, run) in core | Tests | Plan first | ☐ |
| M4.2 | Build-system detection presets | Tests on fixture folders | Qwen | ☐ |
| M4.3 | Run on `GSubprocess`, Output pane; the Build menu with Build (F7), Run (F5) and Stop; no toolbar | Builds Vermilion itself. **◆ DOGFOOD 2: Vermilion edits and builds its own source** | Qwen | ☐ |
| M4.4 | Problem matchers, Problems list, clickable links | Tests on sample compiler output | Qwen | ☐ |
| M4.5 | Review of M4 and the v0.2.0 tag (versions follow the milestones) | Adam signs off. **◆ DOGFOOD 3: Vermilion is Adam's primary editor for this repo; CLion stays only as the host of Qwen (Air plugin / OpenCode)** | Adam | ☐ |

## M5: Agent core (v0.3.0)
**Status**: ☐

- The agent is local first: llama.cpp through the OpenAI-compatible API. The toolset (thirteen tools), staged edits and the pending-changes bar are in ARCHITECTURE.md 7.6. Other providers and MCP are in `docs/ideas.md`.

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M5.1 | Provider interface and one OpenAI-compatible adapter for llama.cpp (Chat Completions, streaming, tool calls) behind a fake transport; fixes the `agent.json` schema (see gate-details) | Tests on recorded streams; real llama.cpp check by Adam | Plan first | ☐ |
| M5.2 | Permission engine in core: presets, rules, hard guards (see gate-details) | Rule-matching tests incl. guards | Plan first | ☐ |
| M5.3a | Tool registry and the read-side tools (see gate-details) | Tests per tool | Qwen | ☐ |
| M5.3b | Edit-side tools and staged edits (see gate-details) | Tests per tool; an agent edit leaves the buffer unsaved and undoable | Plan first | ☐ |
| M5.3c | Build-side tools (see gate-details) | Tests per tool, including the warning cases | Qwen | ☐ |
| M5.4 | Agent loop with limits (50 turns, a token cap). When the server reports that the context is exceeded, it stops with "context full, use /clear" instead of failing or looping | Tests with a scripted provider | Qwen | ☐ |
| M5.6 | Change sets, lean: turn snapshots and whole-turn revert (see gate-details) | Revert a turn restores files | Plan first | ☐ |
| M5.6b | Reverse hunks in core (see gate-details) | Tests on all the edge cases; reverting every hunk of a file gives the snapshot | Plan first | ☐ |
| M5.7 | Chat UI in the Agent tab: Stop, approvals that show the proposed edit as a plain diff, the `ask_user` question card (one multiple-choice question; the agent waits), Copy buttons; status "Processing" while the agent works; the model's reasoning collapsed, expandable to the full text | Manual check | Qwen | ☐ |
| M5.7b | Pending-changes bar in the Agent tab (see gate-details) | Manual check. **◆ DOGFOOD 4: first try of the built-in agent against local llama.cpp on small tasks, with staged edits (see gate-details)** | Qwen | ☐ |
| M5.8 | Setup dialog: provider type, base URL, key variable name, model, connection test, preset (see gate-details) | Endpoint configured without editing files | Plan first | ☐ |
| M5.9 | Chat archive, `/clear` and `/copy`: the transcript model, the archive writer and reader (JSON plus a Markdown copy under `$XDG_DATA_HOME/vermilion/chats/<folder>/`, hardened like the recovery file) and a minimal command parser in the chat entry | Archive round trip tests | Qwen | ☐ |
| M5.10 | `/history`: lists archived chats and opens one read-only | Manual check | Qwen | ☐ |
| M5.11b | Per-hunk Accept and Revert in the DiffView of M2.6: controls beside each changed block, using the reverse-hunk core of M5.6b; a refused revert shows its plain message; a revert is one undo step in the buffer | Manual check | Orchestrator plans, Qwen codes | ☐ |
| M5.12 | Request log and privacy indicator (see gate-details) | Log written; indicator shows the right state | Qwen | ☐ |

## M6: ctxmon UI (v0.4.0)
**Status**: ☐

- The ctxmon UI: settings and server list, I/O, cards and graphs, the terminal (VTE, first built with the cards, M6.3), the standalone `ctxmon`, the ctxmon tab in the IDE, and removal of the Python original.

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M6.1 | ctxmon settings and server list (`--server` specs, warning levels, opacity) in `src/ctxmon/core` | Tests; names fixed | Plan first (rule 10) | ☐ |
| M6.2 | ctxmon I/O on the main loop: log tail with `GFile` async reads, `/slots` and `/props` with libsoup async, a poll timer per server; feeds the M1 logic | Tests with a fake transport and a temp log | Orchestrator plans, Qwen codes | ☐ |
| M6.3 | ctxmon cards and graphs in `src/ctxmon/ui` (UI talk with Adam first), VRAM bar, the terminal (VTE; first built here), `GNotification` warnings. No "Time until full" tile; `Alert::level` becomes `int`, always set (left to the Orchestrator) | Matches the Python behaviour (Adam checks) | Qwen (split as needed) | ☐ |
| M6.4 | Standalone `ctxmon` executable (application ID `io.github.adamjeremy_sk.Ctxmon`; own `GtkApplication` window, same cards, no editor) | Runs without the editor; watches a real llama.cpp server | Qwen | ☐ |
| M6.5 | ctxmon tab in the right-hand stack of `vermilion-ide`, hosting the M6.3 cards; also adds the offer of this tab to the agent's setup dialog (M5.8) | Tab shows the same cards as the standalone program | Qwen | ☐ |
| M6.6 | Remove all Python: the oracle's expected JSON becomes inline C++ case tables in the ctxmon tests (fixture `.log` files stay); delete `ctxmon_gtk.py`, `tools/ctxmon_oracle.py` and the fixtures' `expected/`; `SourceCheck.Repo` fails on any tracked `.py` file | No `.py` file in `git ls-files`; same test count, all passing | Qwen | ☐ |
| M6.7 | Terminal tabs in the editor's bottom panel, reusing the terminal built with ctxmon in M6.3 (the ctxmon terminal serves as the editor's terminal for now; not urgent). Built only if that terminal proves burdensome to use | Shell in the opened folder | Qwen | ☐ |
| M6.8 | v0.4.0 review and tag: ctxmon in both forms (standalone and tab), the terminal, all Python removed | Adam signs off; the v0.4.0 tag (versions follow the milestones) | Adam | ☐ |

## M7: Debugger (v0.5.0)
**Status**: ☐

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M7.1 | DAP client state machine in core (transport injected) | Tests on scripted adapter sessions | Plan first | ☐ |
| M7.2 | Spike: gdb's built-in DAP on a C++ sample | Breakpoint, step, variables work; else fall back to GDB/MI | Adam measures | ☐ |
| M7.3 | Breakpoint gutter marks and current-line marker; breakpoints are kept by file in core, not in the buffer, so closing a tab does not lose them | Manual check | Qwen | ☐ |
| M7.4 | Run control (continue, step over, into, out) from the Debug menu and F5, F10, F11, Shift+F11; no toolbar or button strip | Same | Qwen | ☐ |
| M7.5 | Stack, variables, watches | Same | Qwen | ☐ |
| M7.6 | Terminal integration for the debuggee's stdin and stdout | Same | Qwen | ☐ |

## M8: v0.6.0
**Status**: ☐

| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M8.1 | Cost display with user prices | Totals per conversation and session | Qwen | ☐ |
| M8.2 | Project file: New Project wizard, Adopt as project | Creates `PROJECT.md` and `project.json` | Plan first | ☐ |
| M8.3 | Slash commands and the gate framework | `/gate` shows a copyable prompt; `/audit` issues a verdict | Plan first | ☐ |

## M9: Release (v0.9.0, then 1.0.0)
**Status**: ☐

- Three programs (`vermilion`, `vermilion-ide`, `ctxmon`) are released as `.deb` packages, and as one release zip holding all three prebuilt programs with a custom `install.sh`. The script installs, uninstalls and lets the user choose light, IDE, ctxmon or all three, after checking the system's GTK 4, GtkSourceView 5 and glibc (Depends and Suggests as in ARCHITECTURE.md 7.12).
- No AppImage. The `.rpm` packages and the Fedora VM are parked in `docs/ideas.md` (2026-11-13).
- README and a NOTICE of third-party material (xxHash 0.8.3, BSD-2-Clause, vendored in M2.4, is the first entry).
- **REUSE certification**: every older file gets its header, or a `REUSE.toml` annotation for fixtures, images and other files that cannot hold a comment. `reuse lint` passes and is added to CI. Files made after 2026-11-17 are compliant already.
- A man page `vermilion.1` generated from the manual (the Command-line options topic) when 1.0.0-beta is packaged (no man page before then).
- Version tag and the manual test checklist.
- Then `1.0.0-beta.N` builds run for 8 weeks. The SECURITY.md work (pinned 2026-11-08) happens before the beta starts. `1.0.0-stable` follows, with `-stable` and `-beta` branches.

## Backlog (not scheduled)
Agent shell confinement (Bubblewrap); git integration; multi-cursor; theme system; SSH; session restore; indexed search; plugin hook. All were considered and dropped from the first scope on 2026-11-05. New ideas go to `docs/ideas.md`; the roadmap does not grow before 1.0.0-stable.
