# Roadmap: Vermilion Editor

**Status**: rewritten 2026-11-05 for the greenfield plan. This file is the live roadmap; the old claude.ai Roadmap Doc is obsolete. The architecture is in ARCHITECTURE.md, and the process is in AI_ONBOARDING.md and AGENTS.md.

## How this plan works

**Gates.** Each gate is one step, with evidence and an audit verdict. A gate that adds code also adds its tests. Adam builds, tests and commits, and the work is pushed at the end of each gate.

**Milestone reviews.** Every milestone ends with a review with Adam before the next one starts (AI_ONBOARDING.md section 2, item 7). Its conclusions go to `docs/milestones/M<n>-review.md`.

**The Done column** (Adam, 2026-11-10). ☐ means not done. ☑ means done, followed by the date (YYYY/MM/DD). A note mark (\*, †, ‡, ¶, §, ‖, \#, then doubled: \*\*, ††, ...) points to the notes under that milestone's table, and the marks start again in each milestone. Each milestone also has a Status line.

## Progress

**How it is counted** (Adam, 2026-11-11, weighted). Every milestone has a fixed share of the 100%, set once in the weights line below. A milestone's share is split equally among its own gates, so adding or splitting a gate inside a milestone changes only that milestone's own fraction, never the total. M9 counts as one gate.

**The formula.** Progress is the sum, over all milestones, of weight × (gates ticked ☑ ÷ gates in the milestone). The bar has 20 cells of 5% each, and both the bar and the percentage are rounded down.

**Who keeps it.** The weights are the Orchestrator's estimate of the effort, accepted by Adam, and they change only when Adam decides. The bar is updated with the Done column at every gate commit, and the same line is in README.md.

**Weights**: M0 5 · M1 6 · M2 13 · M3 6 · M4 9 · M5 7 · M6 25 · M7 10 · M8 12 · M9 7 (sum 100).

**Progress**: `███░░░░░░░░░░░░░░░░░` 17% (weighted: M0 and M1 done; M2 6 of 12)

## Versions
| Version | Contents | Milestones |
|---|---|---|
| v0.1 | The light editor `vermilion` and the IDE `vermilion-ide` (right-hand directory sidebar, build and Output pane, Problems, terminal), ctxmon (standalone `ctxmon` and the ctxmon tab) | M0 to M5 |
| v0.2 | Agent core: providers (OpenAI-compatible), tools, permission presets, setup dialog, chat, archive, side-by-side diff, change sets, external-change handling | M6 |
| v0.3 | Debugger (DAP) | M7 |
| v0.4 | Cost display, Anthropic and Gemini adapters, project file, gate framework, MCP | M8 |
| 1.0.0 | Packaging, documentation, notices (M9). Once the whole plan above is implemented and packaged, `1.0.0-beta.N` builds run as a beta for 8 weeks, then `1.0.0-stable` is released; from then on there are `-stable` and `-beta` branches and additional features are considered (`docs/ideas.md`). Development versions before that are `0.x`; the v0.1 to v0.4 labels above are milestone groups (Adam, 2026-11-10). | M9 |
**Dogfooding markers (◆)**: the gates where Adam starts building Vermilion with Vermilion: DOGFOOD 1 at M2.6b (the light editor `vermilion` replaces gedit and Mousepad on Adam's system; set 2026-11-11), 2 at M4.3 (self-build; set 2026-11-05, then Adam's first), 3 at M4.5 (primary editor for this repo), 4 at M6.5 (built-in agent trial; proposed, Adam confirms or moves it).
ctxmon logic (M1) is built early because it is self-contained: the Python original (`ctxmon_gtk.py` in `reference/ctxmon/`, 1,587 lines; logo `reference/ctxmon/ctxmon.svg`) is an executable specification, which makes it the best first job to prove the workflow with the local agent. From 2026-11-07 the port in `src/ctxmon/` is the source of truth and the Python original is frozen as the test oracle (Adam). ctxmon's UI is its own milestone, M5 (Adam, 2026-11-11), placed before the agent core: its settings, I/O, cards and terminal, the standalone program and the ctxmon tab (which needs the right-hand stack from M3).

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
| M0.5d1 | Debt clearing (runs before M0.5c; Adam 2026-11-06, no carried debt): `gtkx` internals into `detail`, `SignalConnection` doc comment, `ObjectPtr` assignment tests, disconnect inside a handler test, `main.cpp` on `gtkx::connect`, purpose comments in `src/ui` and the gtkx tests | Tests pass (58); window still opens (Adam) | Qwen | ☑ 2026/11/06 |
| M0.5d2 | Debt clearing (runs before M0.5c): `result.h` `<utility>` and the `&`/`const&` abort death tests; `redact.cpp` stops at `\v`/`\f` and the four untested paths; purpose comments in `src/core`, `src/agent` and their tests | Tests pass (65) | Qwen | ☑ 2026/11/06 |
| M0.5d3 | One-time `clang-format -i` pass over every `.cpp` and `.h` in `src` and `tests` with the committed `.clang-format` (Adam 2026-11-06); no other change | `clang-format --dry-run --Werror` is clean; tests pass | Qwen | ☑ 2026/11/06 |
| M0.5c1 | Check script `tools/check_sources.sh` as ctest `SourceCheck.Repo` (runs after M0.5d1 to M0.5d3): SPDX header and purpose comment on every source file, plus a file-size report (review above 300 lines, fail above 500; tests and data 500 and 800; AGENTS.md rule 16); `evidence/` added to `.gitignore` (AGENTS.md rule 18) | Script finds no file without a header or purpose comment and reports sizes; 66 tests pass | Qwen | ☑ 2026/11/07 |
| M0.5c2 | Format check `tools/check_format.sh` as ctest `Format.Check`: `clang-format --dry-run --Werror` on every `.cpp` and `.h` in `src` and `tests` (clang-format 18.1.3 on Adam's machine); skipped, not failed, when clang-format is missing | 67 tests pass | Qwen | ☑ 2026/11/07 |

**Notes**

\* Every gate below committed and pushed; 67 tests. Milestone review: `docs/milestones/M0-review.md`.

## M1: Model monitor logic (core, tests from the Python oracle)
**Status**: ☑ 2026/11/10 \*
Revised at the M0 review (2026-11-07): pure logic only, in `src/ctxmon/core` (no GTK; GLib, `GRegex` and json-glib allowed), tests in `tests/ctxmon/`. Functions take text, JSON strings or a directory root; no HTTP, threads or polling (those come in M5.2). Expected values come from the frozen Python original through the oracle script.
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M1.0 | Scaffold `src/ctxmon/core` (library `vermilion_ctxmon_core`) and `tests/ctxmon` (executable `vermilion_ctxmon_tests`); `tools/ctxmon_oracle.py` runs the Python original and writes expected values as JSON to `expected/` in `tests/ctxmon/fixtures/`; first port `fmt_duration` as the smoke test. Each later gate adds its own fixtures (log excerpts in old and new llama.cpp wording, truncation, partial lines; captured `/slots` and `/props` JSON) and one oracle generator | Builds; oracle output committed; 69 tests pass | Qwen | ☑ 2026/11/07 |
| M1.1 | Token lines: `TOKEN_RE` kinds final/prompt/past, last match wins, session floor and truncation (`last_tokens` on text plus offsets) | Tests match the oracle | Qwen | ☑ 2026/11/07 |
| M1.2 | Timing lines (`GEN_RE` with the lookbehind, `PROMPT_RE`), `last_speeds`, `parse_requests` (pairing prompt, generation and the context line after) | Same | Qwen | ☑ 2026/11/07 |
| M1.3 | `/slots` and `/props` parsing on JSON strings (n_ctx, is_processing, n_decoded in both `next_token` shapes; model alias, else file name; `/v1/models` fallback) | Same, on captured JSON | Qwen | ☑ 2026/11/08 |
| M1.4a | `live_speed`, `ServerState` without warnings (histories and pruning, requests, exact versus estimated tokens); the oracle scripts the snapshot sequences | Same; 87 tests | Qwen | ☑ 2026/11/08 |
| M1.4b | `ServerState` warnings: levels 80% and 95% with hysteresis, the first-update rule, time-to-full alert; `down` resets the warning state; `ServerState::update` is split into helpers (it is 91 lines after M1.4a) | Same, alerts and level in the scripted sequences | Qwen | ☑ 2026/11/08 |
| M1.5 | Graph geometry: `graph_segments`, `nice_max`, `graph_span` | Same | Qwen | ☑ 2026/11/08 |
| M1.6 | VRAM readers: AMD sysfs under an injected root, `nvidia-smi` output parsing | Tests on fake sysfs trees and sample output | Qwen | ☑ 2026/11/08 |
| M1.7a | Split from M1.7 by the Orchestrator (gate size). Time-to-full estimate removed (`eta`, its alert, `kEtaWarnSeconds`, `eta_warned_`; oracle drops `eta`, the two time-to-full scenarios and the time-to-full alert, `server_state.json` regenerated, 20 scenarios); three `live_speed` hand tests (equal timestamps, clock going backwards, count not growing). | 103 tests passing | Qwen | ☑ 2026/11/10 † |
| M1.7b | Split again by the Orchestrator (gate size). `server_api`: bodies over 1 MiB (`kMaxBodyBytes`) and nesting deeper than 32 levels (`kMaxJsonDepth`, own scan `json_depth_within` before json-glib; json-glib 1.8.0 was confirmed to crash on 100,000 levels) count as not JSON; a negative `n_ctx` or `n_decoded` counts as missing; six hand tests (negative and huge counts, 1 MiB cap, 1 MB name, depth scan, deep nesting). | 109 tests passing | Qwen | ☑ 2026/11/10 ‡ |
| M1.7c | `vram` `read_file` reads only ordinary files (symlinks followed) of at most 64 bytes, found by reading (sysfs reports 4096 for every file), anything else counts as missing (an oversized vendor file means amd); four `vram` tests on a temporary tree built at run time (64/65 bytes, pipe, link to /dev/zero, oversized vendor, 1 MB nvidia-smi lines); two `log_tokens` and two `timing` tests (1 MB lines, huge numbers, 16 MiB inputs). | 117 tests passing | Qwen | ☑ 2026/11/10 ¶ |

**Notes**

\* Every gate below committed and pushed; 117 tests. Milestone review: `docs/milestones/M1-review.md` (held 2026-11-08, before the M1.7 hardening gates; outcome appended).

† 103 tests.

‡ 109 tests.

¶ 117 tests.

## M2: Editor
**Status**: ☐
**Before any UI gate (Adam, 2026-11-07):** the first gate that builds UI (M2.1, and again before the ctxmon cards M5.3) is preceded by a talk with Adam about his UI specifications; no UI prompt is written before that. **The talk for the M2 window was held 2026-11-10**; its outcome is ARCHITECTURE.md section 7.10 and the HANDOFF.md decision log. The right-hand stack and sidebar (M3) and the ctxmon cards (M5.3) get their own talks.
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M2.0 | Two spikes. (a) GtkSourceView with a 50 MB file; measure open time, scrolling, memory. (b) Look spike (Adam, 2026-11-10): one bare window with a menu bar, a notebook with an editor, a status bar and the classic stylesheet in a light and a dark palette (ARCHITECTURE.md 7.10); it also checks how GTK 4.14 reports the system dark mode. If the CSS fights GTK, fall back to the lighter touch (grey, flat buttons, tight spacing) | Numbers recorded in HANDOFF.md; Adam judges the look by eye before M2.1 starts | Orchestrator plans, Qwen codes, Adam measures and looks | ☑ 2026/11/10 \* |
| M2.1 | Main window: menu bar (File, Edit, View, Help at first), no toolbar (Adam, 2026-11-10), status bar with the Line/Column, INS, Language and Tab Width panels; every command is a `GAction` shared by menu and shortcuts; document tabs (fixed width of 18 characters, ellipsis, tooltip with the path, close button at the tab's right edge (a bold `×` in a raised bevelled square of about 16 px), unsaved marker, middle-click close, drag to reorder, arrows and wheel when they overflow); untitled document at start (lowest free number); closing an edited tab asks Close Without Saving / Cancel (Save joins at M2.2), quitting with edited documents asks the same way; closing the active tab selects its left neighbour (right if it was first), closing a background tab keeps the active one, tabs never stretch to fill the strip; closing the last document leaves a blank window with every document command greyed out (ARCHITECTURE.md 7.10) | Opens and closes tabs; Adam checks the look, including the fused selected tab and the sunken editor well (ARCHITECTURE.md 7.10; light palette only; dark and Follow system arrive with M2.5, Adam 2026-11-10) | Qwen (split as needed: window and actions, then tabs) | ☑ 2026/11/11 † |
| M2.1c | Application icons (Adam, 2026-11-11): rename `res/vedt/Vermilion Editor Icon.svg` to `vermilion-icon.svg` (README reference updated, rule 8 proof); every build renders both SVGs to PNG with `rsvg-convert` (`librsvg2-bin`; configure stops with the package name if it is missing) into the build folder: `vedticon.svg` at 16 and 24 px, `vermilion-icon.svg` at 32, 48, 64, 128 and 256 px, compiled into the resources under `icons/<n>x<n>/apps/`, which GtkApplication adds to the icon theme, under the application ID (GTK picks the small one for the title bar, the large one for the panel, Alt+Tab and About); the resource test checks every size; fully dictated | Icon above the name in About; small icon in the title bar | Qwen | ☑ 2026/11/11 ‡ |
| M2.1d | Continuous integration (Adam, 2026-11-11; build infrastructure, ahead of M2.2): GitHub Actions workflow `.github/workflows/ci.yml` on every push and pull request to `main`: a clean Ubuntu 24.04 runner installs the README's packages (without `clang-tidy`), configures, builds and runs the full `ctest`, including `SourceCheck.Repo` and `Format.Check`; UI tests that need a display will add `xvfb` | The first run on GitHub is green | Qwen | ☑ 2026/11/11 ¶ |
| M2.2 | Open and save, encodings, line endings (Adam's decisions of 2026-11-11). **Open**: before a file loads, the core `TextScanner` reads it once in 64 KiB blocks ("Checking … P%") and decides the encoding (strict whole-file UTF-8 check; a UTF-8 or UTF-16 byte-order mark; Windows-1252, or Latin-1 when one of its five undefined bytes occurs), the line endings (a file with several kinds shows "Mixed" and saves with the ending most lines use), the longest line and whether the file is binary (a NUL byte). GtkSourceView then loads with exactly that encoding and no language; the language is set after the load (M2.0a). A binary file, or a line longer than 350,000 characters (measured by Adam: 500,000 still felt fine; a setting at M2.5), asks "Open Anyway / Cancel"; a binary file opened anyway, and one whose bytes had to be escaped, are read-only. Word wrap is off when a file opens, switched on per document, never asked and never a saved default. The dialogs start in the home folder. **Save**: File → Save (Ctrl+S) and Save As… (Ctrl+Shift+S) go through `GtkSourceFileSaver` (a new copy beside the original, flushed and swapped in, so any failure leaves the original; no backup files); the implicit trailing newline is off so a file round-trips byte for byte; the byte-order mark stays in the text as U+FEFF; a file changed on disk asks "Save Anyway / Cancel"; Save As refuses a path open in another tab; the document is locked while it loads or saves; errors keep it unsaved and give the reason. **Questions**: closing an edited tab asks Save / Close Without Saving / Cancel (untitled: Save As first, cancelling it cancels the close); quitting asks about each edited document in turn, selecting its tab, and Cancel stops the quit. **Status bar menus**: the Encoding panel opens "Reopen as" (UTF-8, UTF-16 LE and BE, Windows-1252, Latin-1; with unsaved edits it asks Save and Reopen / Discard Changes and Reopen / Cancel) and "Save as" (UTF-8, UTF-8 with BOM, UTF-16 LE and BE, Windows-1252, Latin-1); the Line Ending panel converts to LF, CRLF or CR at the next save. **Measured** (Adam): a 38.9 MB file saves in 6.5 s with the window usable. First gate of the Qwen-writes-implementations trial: Qwen wrote the core `.cpp` files, `document_saver.cpp` and the status bar; the Orchestrator dictated headers, tests and the rest of the UI (Adam, 2026-11-11) | Core tests for the scanner, formats, document list and messages; Adam's 18-point run of open and save | Qwen | ☑ 2026/11/11 § |
| M2.2b | Help → Index (Adam, 2026-11-11): every `docs/manual/*.md` collected by CMake into the resources (a new topic needs no code change) and shown in a separate read-only window (F1; a second Help → Index brings it forward) that opens on `index.md`: labels and grids, links between topics replace the page, Back and Index buttons (Alt+Left) over a tested page history; a small renderer for a Markdown subset (headings, paragraphs with joined source lines, lists, bold, inline code, tables, links to topics; anything else shows as plain text) tested without a display; a test that every manual file is in the program, linked from the index and inside the subset; replaces a separate Help → Keyboard Shortcuts item | Every topic opens and reads correctly; renderer, history and manual tests | Qwen writes the renderer, the page history and the window from specs; headers, tests and wiring dictated | ☑ 2026/11/11 ‖ |
| M2.2c | Crash recovery (Adam, 2026-11-11: a basic editor function, not a new feature; talk held and decided the same night). Every edited document, untitled ones included, is copied to a recovery file in `$XDG_STATE_HOME/vermilion/recovery/<program>/` (`light` for `vermilion`, `ide` for `vermilion-ide`) 5 s after typing stops and at the latest 30 s after the first unsaved change; saving or Close Without Saving deletes the copy. A large document times its own main-loop freeze and the next copy waits at least 20 times that; above a ceiling size there is no copy and the status bar says so; if Adam's run of the wiring on the 39 MB file (the copy logs its hold; nothing is measured outside the repo) shows a freeze over about 100 ms, the copy is taken in 1 MB slices on idle callbacks; the ceiling is counted in characters and fixed from the same log. Files and folder are private (0700 and 0600) and barred to the agent's file tools. At start-up each copy left behind is offered in turn with the time of the copy and of the file: Recover, Discard or Decide Later (Recover All when more than one waits). No Compare button until the diff view exists (M6.9). The file format is planned with Adam first (AGENTS.md rule 10) | A killed editor offers the unsaved text at the next start; core tests | Qwen | ☐ |
| M2.3 | Search, replace (regex), go to line; the Search menu | Manual check plus core tests for the matcher | Qwen | ☐ |
| M2.4 | File monitor and the external-change policy | Edit on disk reloads a clean buffer; dirty buffer shows the banner | Orchestrator plans, Qwen codes | ☐ |
| M2.5 | Settings window (classic OK, Cancel, Apply) and key file: colour mode Light, Dark or Follow system with the two classic palettes, GtkSourceView schemes (`classic` and a dark one), editor font (default: system monospace), tab width; the Tools menu | Settings persist; Adam checks light and dark | Plan first (rule 10) | ☐ |
| M2.6 | Window size restored; start state (a new untitled document by default; or reopen the last document, a single file; or a blank window); no tab limit (dropped by Adam 2026-11-10, parked in `docs/ideas.md`). The folder items moved to M3.0 (Adam, 2026-11-10) | Restored on start | Qwen | ☐ |
| M2.6b | The light program (Adam, 2026-11-11; replaces the feather mode of 2026-11-10; ARCHITECTURE.md 7.12): the M2 code moves into a static library and `vermilion` becomes a thin `main` over it (no `--feather` flag); `vermilion [file...]`, files from the command line and from "Open with", its own application ID, a desktop entry for text files, the single-instance rule, quits when the last document closes, shows only File, Edit, Search, View and Help, treats the "blank window" start state as untitled, recovery folder `light`; a ctest checks that the executable links no VTE, libsoup, libsecret, agent or ctxmon code; startup time measured; the command-line options are documented in a new manual topic, Command-line options (Adam, 2026-11-11), where every later flag goes too | Opens text files from the file manager; the link check passes; Adam measures startup. **◆ DOGFOOD 1: `vermilion` replaces gedit and Mousepad on Adam's system** | Orchestrator plans, Qwen codes | ☐ |

**Notes**

\* 117 tests; results in ARCHITECTURE.md 7.1 and 7.10.

† 131 tests. Run as five steps in one prompt, then a fresh session for STEP 4b (fixes after Adam tried the tabs: fixed tab width, background close, left neighbour, Edit commands on the active editor) and STEP 5 with the menu colours; fix 1 shortened Help → About.

‡ 132 tests. Two steps (the rename, then rendering and use). Fix 1 after Adam opened About: GTK gives the focus to the program name when About opens, which selects it, so the stylesheet draws selected text in About without a highlight (Adam's option B over leaving it or moving the focus in code). On Adam's XFCE with Greybird the title bar shows the window-menu arrow, not an application icon, so the badge is used where XFCE asks for small icons.

¶ 132 tests. One step; the workflow was checked as YAML before it was issued and is first run by GitHub after the push.

§ 205 tests. Part A (core: scanner, formats, file documents, open messages; 165 then 181 tests), M2.2 UI (the open flow, STEPS 3 and 4; 185 tests), the save part (STEP 5 core, 205 tests; STEPS 6 and 7 the saver, the status bar menus, Save, Save As and the questions) and one fix (the dialogs start in the home folder; Save and Reopen). Adam ran 18 checks of open and save by hand; the byte checks all matched. Long-line limit 350,000 characters; saving 38.9 MB took 6.5 s.

‖ 259 tests. Six one-step prompts (the spans, the page blocks, the page history, the manual as a second resource with two tests about the real files, the help window, the wiring); Qwen wrote `manual_spans.cpp`, `manual_page.cpp`, `page_history.cpp` and `help_window.cpp` from specifications and passed audits with a differential fuzz against the Orchestrator's reference implementations; one fix (comments moved off statements, an unused include). The manual gained the topic "Using the manual".

## M3: Directory sidebar (right-hand side)
**Status**: ☐
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M3.0 | The `vermilion-ide` executable appears (ARCHITECTURE.md 7.12: a thin `main` over the editor base, application ID `io.github.adamjeremy_sk.VermilionIde`, recovery folder `ide`; Adam, 2026-11-11, timing proposed by the Orchestrator), then Open Folder, Recent Folders, Close Folder and restoring the last folder, with their File menu items (moved from M2.6, Adam 2026-11-10: menus appear only when their feature exists) | Folder restored on start | Qwen | ☐ |
| M3.1 | FileOps interface in core with a fake, plus tests | Acceptance tests in the sidebar requirement (`docs/requirements/`) pass | Qwen | ☐ |
| M3.2 | Tree widget, lazy loading, folders first | Opens a folder, lists files | Qwen | ☐ |
| M3.3 | Filter, hidden-files toggle, Sync, Refresh, keys | Manual check against the requirement | Qwen | ☐ |
| M3.4 | Context menus, rename, delete (trash), copy, move, drag and drop | Same | Qwen | ☐ |
| M3.5 | Folder monitor | Tree follows external changes | Qwen | ☐ |

## M4: Build, Output, Problems (v0.1)
**Status**: ☐
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M4.1 | Project file model (build, run) in core | Tests | Plan first | ☐ |
| M4.2 | Build-system detection presets | Tests on fixture folders | Qwen | ☐ |
| M4.3 | Run on `GSubprocess`, Output pane; the Build menu with Build (F7), Run (F5) and Stop; no toolbar (Adam, 2026-11-10) | Builds Vermilion itself. **◆ DOGFOOD 2: Vermilion edits and builds its own source** | Qwen | ☐ |
| M4.4 | Problem matchers, Problems list, clickable links | Tests on sample compiler output | Qwen | ☐ |
| M4.5 | Review of M4 (Adam, 2026-11-11: the v0.1 tag moved to the end of M5, after the ctxmon UI) | Adam signs off. **◆ DOGFOOD 3: Vermilion is Adam's primary editor for this repo; CLion stays only as the host of Qwen (Air plugin / OpenCode)** | Adam | ☐ |

## M5: ctxmon UI (v0.1)
**Status**: ☐
Its own milestone since 2026-11-11 (Adam), placed before the agent core. It holds the gates that were M2.7 to M2.11 (now M5.1 to M5.4 and M5.6), the ctxmon tab (was M3.6, now M5.5), the conditional terminal tabs (was M4.5, now M5.7) and the v0.1 tag (was part of M4.6, now M5.8). The terminal (VTE) is first built with the ctxmon cards, M5.3.
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M5.1 | ctxmon settings and server list (`--server` specs, warning levels, opacity) in `src/ctxmon/core` | Tests; names fixed | Plan first (rule 10) | ☐ |
| M5.2 | ctxmon I/O on the main loop: log tail with `GFile` async reads, `/slots` and `/props` with libsoup async, a poll timer per server; feeds the M1 logic | Tests with a fake transport and a temp log | Orchestrator plans, Qwen codes | ☐ |
| M5.3 | ctxmon cards and graphs in `src/ctxmon/ui` (shared widget set; a talk with Adam about the UI specifications comes first), VRAM bar, the terminal (VTE; this is where Vermilion's terminal is first built, Adam 2026-11-07), `GNotification` warnings; no "Time until full" tile (dropped, Adam 2026-11-08); `Alert::level` changes from `std::optional<int>` to `int` (always set; Adam left it to the Orchestrator, 2026-11-10) | Matches the Python behaviour (Adam checks) | Qwen (split as needed) | ☐ |
| M5.4 | Standalone `ctxmon` executable (renamed from `vermilion-ctxmon`, Adam 2026-11-11; application ID `io.github.adamjeremy_sk.Ctxmon`; own `GtkApplication` window, same cards, no editor) | Runs without the editor; watches a real llama.cpp server | Qwen | ☐ |
| M5.5 | ctxmon tab in the right-hand stack of `vermilion-ide`, hosting the M5.3 cards | Tab shows the same cards as the standalone program | Qwen | ☐ |
| M5.6 | Remove all Python (Adam, 2026-11-07): convert the oracle's expected JSON into inline C++ case tables in the ctxmon tests (the fixture `.log` files stay); delete `ctxmon_gtk.py` in `reference/ctxmon/`, `tools/ctxmon_oracle.py` and `expected/` in `tests/ctxmon/fixtures/`; tests no longer link json-glib unless another test needs it; `SourceCheck.Repo` fails on any tracked `.py` file | No `.py` file in `git ls-files`; same test count, all passing | Qwen | ☐ |
| M5.7 | Terminal tabs in the editor's bottom panel, reusing the terminal built with ctxmon in M5.3 (Adam, 2026-11-11: the terminal is built with ctxmon and this is not urgent; Adam, 2026-11-08: the ctxmon terminal serves as the editor's terminal for now; this gate is built only if that terminal proves burdensome to use) | Shell in the opened folder | Qwen | ☐ |
| M5.8 | v0.1 review and tag: the editor, ctxmon in both forms, the sidebar, build and Output, Problems, the terminal | Adam signs off; the v0.1 tag (moved here from the end of M4, Adam 2026-11-11) | Adam | ☐ |

## M6: Agent core (v0.2)
**Status**: ☐
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M6.1 | Provider interface, OpenAI-compatible adapter (streaming, tool calls) behind a fake transport | Tests on recorded streams; real llama.cpp check by Adam | Plan first | ☐ |
| M6.2 | Tool registry and the eight tools | Tests per tool | Qwen | ☐ |
| M6.3 | Permission engine: presets, rules, hard guards, never-send list | Rule-matching tests incl. guards | Plan first | ☐ |
| M6.4 | Agent loop with limits | Tests with a scripted provider | Qwen | ☐ |
| M6.5 | Chat UI in the Agent tab: Stop, approvals, Copy buttons; while the agent works the status reads "Processing"; the model's reasoning is collapsed and expands to the full text (Adam, 2026-11-10) | Manual check. **◆ DOGFOOD 4 (proposed): first try of the built-in agent against local llama.cpp on small tasks; CLion/OpenCode remains the primary agent until Adam judges it good enough** | Qwen | ☐ |
| M6.6 | Setup dialog and keyring | Endpoint configured without editing files | Plan first | ☐ |
| M6.7 | `/clear`, archive, `/history`, `/copy` | Archive round trip tests | Qwen | ☐ |
| M6.8 | Diff engine (Myers, aligned rows, word-level) | Tests incl. edge cases | Qwen | ☐ |
| M6.9 | `DiffView` widget | Side by side, folding, per-hunk controls; adds the Compare button to the crash-recovery question (M2.2c) | Orchestrator plans, Qwen codes | ☐ |
| M6.10 | Change sets, undo grouping, snapshots, buffer-version checks | Revert a turn restores files | Plan first | ☐ |
| M6.11 | Request log and privacy indicator | Log written; indicator shows local or remote | Qwen | ☐ |
| M6.12 | Context budget and summarising | Long conversation stays under the budget | Plan first | ☐ |

## M7: Debugger (v0.3)
**Status**: ☐
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M7.1 | DAP client state machine in core (transport injected) | Tests on scripted adapter sessions | Plan first | ☐ |
| M7.2 | Spike: gdb's built-in DAP on a C++ sample | Breakpoint, step, variables work; else fall back to GDB/MI | Adam measures | ☐ |
| M7.3 | Breakpoint gutter marks and current-line marker; breakpoints are kept by file in core, not in the buffer, so closing a tab does not lose them | Manual check | Qwen | ☐ |
| M7.4 | Run control (continue, step over, into, out) from the Debug menu and F5, F10, F11, Shift+F11; no toolbar or button strip (Adam, 2026-11-10) | Same | Qwen | ☐ |
| M7.5 | Stack, variables, watches | Same | Qwen | ☐ |
| M7.6 | Terminal integration for the debuggee's stdin and stdout | Same | Qwen | ☐ |

## M8: v0.4
**Status**: ☐
| Gate | Work | Done when | Who | Done |
|---|---|---|---|---|
| M8.2 | Cost display with user prices | Totals per conversation and session | Qwen | ☐ |
| M8.3 | Anthropic adapter | Same tests as M6.1 | Plan first | ☐ |
| M8.4 | Gemini (compatible endpoint check, native if needed) | Same | Plan first | ☐ |
| M8.5 | Project file: New Project wizard, Adopt as project | Creates `PROJECT.md` and `project.json` | Plan first | ☐ |
| M8.6 | Slash commands and the gate framework | `/gate` shows a copyable prompt; `/audit` issues a verdict | Plan first | ☐ |
| M8.7 | MCP server (stdio) | A client lists and calls the tools | Plan first | ☐ |
| M8.8 | MCP client (stdio) | The agent calls an outside tool | Plan first | ☐ |

## M9: Release
**Status**: ☐
three programs (`vermilion`, `vermilion-ide`, `ctxmon`) released as `.deb` packages, `.rpm` packages (built and tested in a Fedora VM) and one release zip holding all three prebuilt programs with a custom `install.sh` that installs, uninstalls and lets the user choose light, IDE, ctxmon or all three, after checking the system's GTK 4, GtkSourceView 5 and glibc (Depends and Suggests as in ARCHITECTURE.md 7.12; Adam, 2026-11-11 and 2026-11-12: no AppImage), README, NOTICE of third-party material, a man page `vermilion.1` generated from the manual (the Command-line options topic) when 1.0.0-beta is packaged (Adam, 2026-11-11: no man page before then), version tag, manual test checklist. Then `1.0.0-beta.N` builds run for 8 weeks; the SECURITY.md work (pinned 2026-11-08) happens before the beta starts (Adam, 2026-11-10); `1.0.0-stable` follows, with `-stable` and `-beta` branches.

## Backlog (not scheduled)
Agent shell confinement (Bubblewrap); git integration; multi-cursor; theme system; SSH; session restore; indexed search; plugin hook. All were considered and dropped from the first scope on 2026-11-05. New ideas go to `docs/ideas.md`; the roadmap does not grow before 1.0.0-stable (Adam, 2026-11-10).
