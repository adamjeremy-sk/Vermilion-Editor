# Roadmap: Vermilion Editor
**Status**: rewritten 2026-11-05 for the greenfield plan; this file is the live roadmap (the old claude.ai Roadmap Doc is obsolete). Architecture is in ARCHITECTURE.md; the process is in AI_ONBOARDING.md and AGENTS.md. Gate rules: each gate is one step with evidence and an audit verdict; a gate that adds code also adds its tests; Adam builds, tests and commits; push at each gate end.

## Versions
| Version | Contents | Milestones |
|---|---|---|
| v0.1 | Editor, right-hand directory sidebar, build and Output pane, Problems, terminal | M0 to M4 |
| v0.2 | Agent core: providers (OpenAI-compatible), tools, permission presets, setup dialog, chat, archive, side-by-side diff, change sets, external-change handling | M5 |
| v0.3 | Debugger (DAP) | M6 |
| v0.4 | CTXMon tab and standalone monitor, cost display, Anthropic and Gemini adapters, project file, gate framework, MCP | M7 |
| v1.0 | Packaging, documentation, notices | M8 |
**Dogfooding markers (◆)**: the gates where Adam starts building Vermilion with Vermilion: DOGFOOD 1 at M4.3 (self-build; Adam's first one, set 2026-11-05), 2 at M4.6 (primary editor), 3 at M5.5 (built-in agent trial; proposed, Adam confirms or moves it).
ctxmon logic (M1) is built early because it is self-contained: the Python original (`reference/ctxmon/ctxmon_gtk.py`, 1,587 lines; logo `reference/ctxmon/ctxmon.svg`) is an executable specification, which makes it the best first job to prove the workflow with the local agent.

## M0: Reset and foundation
| Gate | Work | Done when | Who |
|---|---|---|---|
| M0.1 | Reset the tree: one `git rm -r` of everything except the kept files; GPL-3 `LICENSE`; remove the `upstream` remote; commit docs | `git ls-files` shows only the kept files; tree clean | Adam, Orchestrator checks |
| M0.2 | CMake skeleton: top level, `src/core`, `src/agent`, `src/ui`, `tests`, GoogleTest target, `.clang-format`, `.gitignore`, a `GtkApplication` window with the application ID | Configures and builds; empty window opens; `ctest` runs one trivial test | Qwen |
| M0.3 | Fix target linkage (`-fno-exceptions` must not reach the tests); `src/ui/gtkx` library with `ObjectPtr` and string helpers, tested without a display | Compiles; `vermilion_gtkx_tests` pass; compile commands show tests without `-fno-exceptions` | Qwen |
| M0.4 | `gtkx` signal connections by lambda (RAII disconnect) | Tests with `GListStore` and `GSimpleAction` pass | Qwen |
| M0.5a | `Error` and `Result<T>` types in core (`Status` = `Result<Unit>`), tested incl. death tests for misuse | Tests pass; header compiles under `-fno-exceptions` | Qwen |
| M0.5b1 | `redact_secrets(std::string_view)` in core: masks API keys, bearer tokens and secret-named fields (pure text, no GLib) | Tests pass; header and source compile under `-fno-exceptions` | Qwen |
| M0.5b2 | Log layer over GLib `g_log` (domain `vermilion`), every message passed through `redact_secrets`; links GLib into core. A thin layer is allowed because it adds redaction (wrapper rule) | Tests capture messages with a log handler; no secrets in log output | Qwen |
| M0.5c | Check script: SPDX headers on every source file, plus a file-size report (review above 300 lines, fail above 500; tests and data 500 and 800; AGENTS.md rule 16) | Script finds no file without a header and reports sizes | Qwen |

## M1: Model monitor logic (core, tests from the Python oracle)
| Gate | Work | Done when | Who |
|---|---|---|---|
| M1.1 | Log tail and token parsing (`n_tokens`, `n_past`, session floor) | Tests on fixture logs match the Python output | Qwen |
| M1.2 | Timing lines and finished-request history | Same | Qwen |
| M1.3 | `/slots` and `/props` parsing, live speed | Same, on captured JSON | Qwen |
| M1.4 | Poller model, warnings (80%, 95%, time to full) | Same | Qwen |
| M1.5 | VRAM readers (AMD sysfs, `nvidia-smi`) | Tests on fake sysfs trees and sample output | Qwen |

## M2: Editor
| Gate | Work | Done when | Who |
|---|---|---|---|
| M2.0 | Spike: GtkSourceView with a 50 MB file; measure open time, scrolling, memory | Numbers recorded in HANDOFF.md | Orchestrator plans, Qwen codes, Adam measures |
| M2.1 | Main window, notebook, editor tab | Opens and closes tabs | Qwen |
| M2.2 | Open and save, encodings, line endings | Round trip tests in core for the conversions | Qwen |
| M2.3 | Search, replace (regex), go to line | Manual check plus core tests for the matcher | Qwen |
| M2.4 | File monitor and the external-change policy | Edit on disk reloads a clean buffer; dirty buffer shows the banner | Orchestrator plans, Qwen codes |
| M2.5 | Settings (key file), style schemes, system dark mode | Settings persist | Plan first (rule 10) |
| M2.6 | Recent folders, last folder and window size | Restored on start | Qwen |

## M3: Directory sidebar (right-hand side)
| Gate | Work | Done when | Who |
|---|---|---|---|
| M3.1 | FileOps interface in core with a fake, plus tests | Acceptance tests in `docs/requirements/directory-sidebar.md` pass | Qwen |
| M3.2 | Tree widget, lazy loading, folders first | Opens a folder, lists files | Qwen |
| M3.3 | Filter, hidden-files toggle, Sync, Refresh, keys | Manual check against the requirement | Qwen |
| M3.4 | Context menus, rename, delete (trash), copy, move, drag and drop | Same | Qwen |
| M3.5 | Folder monitor | Tree follows external changes | Qwen |

## M4: Build, Output, Problems, Terminal (v0.1)
| Gate | Work | Done when | Who |
|---|---|---|---|
| M4.1 | Project file model (build, run) in core | Tests | Plan first |
| M4.2 | Build-system detection presets | Tests on fixture folders | Qwen |
| M4.3 | Run on `GSubprocess`, Output pane | Builds Vermilion itself. **◆ DOGFOOD 1: Vermilion edits and builds its own source** | Qwen |
| M4.4 | Problem matchers, Problems list, clickable links | Tests on sample compiler output | Qwen |
| M4.5 | VTE terminal tabs | Shell in the opened folder | Qwen |
| M4.6 | v0.1 review and tag | Adam signs off. **◆ DOGFOOD 2: Vermilion is Adam's primary editor for this repo; CLion stays only as the host of Qwen (Air plugin / OpenCode)** | Adam |

## M5: Agent core (v0.2)
| Gate | Work | Done when | Who |
|---|---|---|---|
| M5.1 | Provider interface, OpenAI-compatible adapter (streaming, tool calls) behind a fake transport | Tests on recorded streams; real llama.cpp check by Adam | Plan first |
| M5.2 | Tool registry and the eight tools | Tests per tool | Qwen |
| M5.3 | Permission engine: presets, rules, hard guards, never-send list | Rule-matching tests incl. guards | Plan first |
| M5.4 | Agent loop with limits | Tests with a scripted provider | Qwen |
| M5.5 | Chat UI in the Agent tab: Stop, approvals, Copy buttons | Manual check. **◆ DOGFOOD 3 (proposed): first try of the built-in agent against local llama.cpp on small tasks; CLion/OpenCode remains the primary agent until Adam judges it good enough** | Qwen |
| M5.6 | Setup dialog and keyring | Endpoint configured without editing files | Plan first |
| M5.7 | `/clear`, archive, `/history`, `/copy` | Archive round trip tests | Qwen |
| M5.8 | Diff engine (Myers, aligned rows, word-level) | Tests incl. edge cases | Qwen |
| M5.9 | `DiffView` widget | Side by side, folding, per-hunk controls | Orchestrator plans, Qwen codes |
| M5.10 | Change sets, undo grouping, snapshots, buffer-version checks | Revert a turn restores files | Plan first |
| M5.11 | Request log and privacy indicator | Log written; indicator shows local or remote | Qwen |
| M5.12 | Context budget and summarising | Long conversation stays under the budget | Plan first |

## M6: Debugger (v0.3)
| Gate | Work | Done when | Who |
|---|---|---|---|
| M6.1 | DAP client state machine in core (transport injected) | Tests on scripted adapter sessions | Plan first |
| M6.2 | Spike: gdb's built-in DAP on a C++ sample | Breakpoint, step, variables work; else fall back to GDB/MI | Adam measures |
| M6.3 | Breakpoint gutter marks and current-line marker | Manual check | Qwen |
| M6.4 | Run control (continue, step over, into, out) | Same | Qwen |
| M6.5 | Stack, variables, watches | Same | Qwen |
| M6.6 | Terminal integration for the debuggee's stdin and stdout | Same | Qwen |

## M7: v0.4
| Gate | Work | Done when | Who |
|---|---|---|---|
| M7.1 | CTXMon cards and graphs as a shared widget set; the CTXMon tab in the right-hand stack | Matches the Python behaviour | Qwen |
| M7.1b | Standalone `vermilion-ctxmon` executable (own window, same cards, no editor); may be built earlier, any time after M1 and the M2.1 window, if Adam wants it sooner | Runs without the editor; watches a real llama.cpp server | Qwen |
| M7.2 | Cost display with user prices | Totals per conversation and session | Qwen |
| M7.3 | Anthropic adapter | Same tests as M5.1 | Plan first |
| M7.4 | Gemini (compatible endpoint check, native if needed) | Same | Plan first |
| M7.5 | Project file: New Project wizard, Adopt as project | Creates `PROJECT.md` and `project.json` | Plan first |
| M7.6 | Slash commands and the gate framework | `/gate` shows a copyable prompt; `/audit` issues a verdict | Plan first |
| M7.7 | MCP server (stdio) | A client lists and calls the tools | Plan first |
| M7.8 | MCP client (stdio) | The agent calls an outside tool | Plan first |

## M8: Release
`.deb` or AppImage, README, NOTICE of third-party material, version tag, manual test checklist.

## Backlog (not scheduled)
Agent shell confinement (Bubblewrap); git integration; multi-cursor; theme system; SSH; session restore; indexed search; plugin hook. All were considered and dropped from the first scope on 2026-11-05.
