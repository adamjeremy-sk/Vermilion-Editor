# ARCHITECTURE.md: Vermilion Editor
**Status**: rewritten 2026-11-05 for the greenfield plan. Sections marked *Decided* are Adam's decisions; *Proposed* sections are the Orchestrator's recommendations awaiting Adam; names marked *(name proposed)* are placeholders fixed at the gate that creates them. The project is written from scratch.

## 1. What this is  *(Decided)*
Vermilion Editor is a native Linux text and code editor written from scratch in C++17 on GTK 4 and GtkSourceView 5, with a built-in AI agent, a project build system integration, a debugger and a terminal. It is released under GPL-3.0-or-later. Target system: Ubuntu 24.04 (GTK 4.14, GLib 2.80, gdb 15.1). Linux only.

## 2. Decisions in force  *(Decided)*
| Topic | Decision |
|---|---|
| Language | C++17. GTK 4 and GLib are used through their plain C APIs. No gtkmm, no libadwaita, no language bindings, no vendor SDKs. |
| Wrapper rule | Nothing that exists only to wrap another API: talk to the thing directly (C API, subprocess, or wire protocol). Libraries that provide a widget or a service (GtkSourceView, VTE, libsoup, json-glib, libsecret) are allowed. |
| Licence | GPL-3.0-or-later. Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later` and `// Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)`. |
| Editor widget | GtkSourceView 5 (syntax, line numbers, gutter marks, search, undo, style schemes). No custom text engine. |
| Build system of Vermilion | CMake, system packages through pkg-config. |
| Platform | Linux, Wayland and X11 through GTK. English UI only. No telemetry. |
| Process | Gate system (AI_ONBOARDING.md, AGENTS.md). Adam builds, tests and commits. |
| Application ID | `io.github.adamjeremy_sk.Vermilion` |
| Dropped | Macros, user-tool slots, column mode, bookmarks, templates, printing, FTP, git integration, multi-cursor, theme system beyond GtkSourceView style schemes and the system dark mode, SSH, session restore beyond last folder and window size, indexed project search, plugin system. |

## 3. The product  *(Decided)*
```
+------------------------------------------------------+------------------------+
| tabs: main.cpp | util.h | ...                        | Files | Agent | CTXMon |
|------------------------------------------------------|------------------------|
|                                                      | v project/             |
|   editor (syntax colours, line numbers,              |   v src/               |
|   breakpoint dots, current-line arrow)               |     main.cpp           |
|                                                      |     util.h             |
|                                                      |   CMakeLists.txt       |
|------------------------------------------------------|------------------------|
| Output | Problems | Terminal | Debug      [Build][Run]| Agent: chat, diff;     |
|   build log, clickable errors                        | CTXMon: server monitor |
+------------------------------------------------------+------------------------+
```
One window, one opened folder at a time. Right-hand stack: **Files** (directory sidebar), **Agent** and **CTXMon** (model monitor) tabs. Bottom panel: Output, Problems, Terminal (VTE, several tabs), Debug (stack, variables, breakpoints, watches).

## 4. Layers and layout  *(layers Decided; directories fixed at M0.2)*
Dependency direction: `ui -> agent -> core`; `ui -> core`; `ctxmon/core -> core`; `ui -> ctxmon/ui -> ctxmon/core`. Core, agent and ctxmon core never include GTK, Pango, GtkSourceView or VTE headers. GLib, GIO and json-glib are allowed there. Check: a grep of `src/core`, `src/agent` and `src/ctxmon/core` for `gtk/`, `gdk`, `pango`, `gtksourceview`, `vte` returns nothing.
| Library | Directory | Contents |
|---|---|---|
| `vermilion_core` | `src/core/` | diff engine (Myers, produces aligned rows for side-by-side), permission engine (presets, rules, hard guards), settings and project-file models, build presets and problem matchers, DAP client state machine (transport injected), slash-command and gate-template engine, cost calculator, chat archive. |
| `vermilion_agent` | `src/agent/` | provider adapters (OpenAI-compatible, Anthropic, Gemini), agent loop, tool registry, MCP server and client. HTTP through libsoup 3 behind an injectable transport so tests use a fake. |
| `vermilion` (executable) | `src/ui/` | GTK application, window, tabs, editor view, directory sidebar, bottom panels, terminal, agent tab and setup dialog, diff widget, debugger panels, the CTXMon tab (hosts the cards from `src/ctxmon/ui`); a small helper layer (`src/ui/gtkx/`: GObject reference pointer, signal connection by lambda, string conversion). |
| `vermilion_ctxmon_core` *(name proposed, fixed at M1.0)* | `src/ctxmon/core/` | ctxmon logic, UI-free: log token and timing parsing, request history, `/slots` and `/props` parsing, live speed, server state and warnings, graph geometry, VRAM readers. Pure functions on text, JSON strings and file trees; the I/O that feeds them comes in M2. |
| ctxmon UI | `src/ctxmon/ui/` | (M2) The server cards and graphs shared by the CTXMon tab and the standalone `vermilion-ctxmon` executable, and that executable's `main`. |
| tests | `tests/`, `tests/ctxmon/` | GoogleTest, one test file per core, agent or ctxmon module, no display needed. ctxmon tests take their expected values from the Python oracle (M1.0). UI is kept thin and is checked by hand. |

Repository checks live in `tools/` and run as ctest tests with every test run: `check_sources.sh` (`SourceCheck.Repo`: SPDX and copyright header, purpose comment, file size per AGENTS.md rule 16) and `check_format.sh` (`Format.Check`: `clang-format --dry-run --Werror` on every `.cpp` and `.h`; skipped when clang-format is missing).

## 5. Conventions  *(Decided 2026-11-05 as part of the language decision; details Proposed)*
- **Errors:** no C++ exceptions in project code. An exception escaping through a GTK callback is undefined behaviour. Use `std::optional` or a small `Result<T>` type; at the GTK boundary use `GError`. Compile with `-fno-exceptions` where the dependencies allow it.
- **Memory:** RAII only. Smart pointers or the `gtkx` helpers for GObject references; no raw `new`/`delete`/`g_free` outside the helpers.
- **Threading:** the GLib main loop plus async I/O (`GSubprocess`, libsoup async API, `GTask`) for network, builds, debugger and monitors. Threads only for heavy computation (large diffs). Widgets are touched from the main thread only. The main loop is never blocked.
- **Paths and data:** `std::filesystem` or GIO. XDG locations for config, data, cache and state. Text is UTF-8; encoding and line endings are explicit at file boundaries.
- **Style:** `clang-format` (`.clang-format` committed at M0.2: LLVM base, indent 4, 100 columns; enforced by ctest `Format.Check`), warnings as errors (`-Wall -Wextra -Werror`), `clang-tidy` where cheap, ASan and UBSan builds run by Adam on request.
- **Size and shape:** one responsibility per file; a source file over 300 lines is reviewed, over 500 is split (tests and data tables 500 and 800); functions over 50 lines are flagged; UI is one widget or panel class per file pair and `window` only assembles panels. Details: AGENTS.md rule 16.
- **Regular expressions:** GLib's `GRegex` (PCRE2: lookbehind, named groups, no exceptions), part of GLib and allowed in every layer; `std::regex` is not used (Adam, 2026-11-07). Later users: ctxmon parsing (M1), editor search (M2.3), problem matchers (M4.4).
- **Identity stability:** never rename config keys, JSON fields, file formats, command names or IDs when only display text changes.

## 6. Dependencies  *(Decided; versions are those of Ubuntu 24.04)*
| Need | Library | Ubuntu package |
|---|---|---|
| UI | GTK 4 | `libgtk-4-dev` |
| Editor widget | GtkSourceView 5 | `libgtksourceview-5-dev` |
| JSON | json-glib | `libjson-glib-dev` |
| HTTP, SSE | libsoup 3 | `libsoup-3.0-dev` |
| Key storage | libsecret | `libsecret-1-dev` |
| Terminal | VTE (GTK 4) | `libvte-2.91-gtk4-dev` |
| Tests | GoogleTest | `libgtest-dev` |
| Tooling | CMake, pkg-config, clang-format, clang-tidy | `cmake pkg-config clang-format clang-tidy build-essential` |
All are LGPL-style or permissive and compatible with GPL-3. New dependencies need Adam's decision.

## 7. Feature designs

### 7.1 Editor  *(Decided scope, Proposed detail)*
Tabs (`GtkNotebook`), open and save, encodings through GLib converters (UTF-8 default), line endings detected and preserved (LF for new files), search and replace with `GRegex`, go to line, line numbers, GtkSourceView language detection and style schemes (follows the system dark mode). Gutter marks for breakpoints and the current debug line. File monitoring (see 7.6 "External changes"). Large-file behaviour is measured once (50 MB file) before it is promised.

### 7.2 Directory sidebar  *(Decided)*
Right-hand side, first tab of the right stack. Behaviour is specified in `docs/requirements/directory-sidebar.md`. GTK 4 mapping: `GtkTreeListModel`, `GtkTreeExpander`, `GtkListView`, `GFileEnumerator` (async), `GFileMonitor`. Filesystem work sits in core behind a small interface with a fake for tests.

### 7.3 Build  *(Decided scope, Proposed detail)*
Per-folder configuration in `.vermilion/project.json`: build command, run command, working directory, environment. Presets detect CMake, Make, Ninja, Cargo, Gradle and Bazel from the files in the folder. Output goes to the Output pane; error lines from gcc, clang, rustc and javac are parsed by problem matchers (regular expressions, user-extendable in JSON) into the Problems list and clickable `file:line:column` links. Commands run on `GSubprocess`; the agent can call the same entry point.

### 7.4 Terminal  *(Decided)*
VTE terminal tabs in the bottom panel, shell from `$SHELL`, started in the opened folder. Run and Debug use a terminal tab so the program has a real stdin and stdout. The ctxmon panel offers the same terminal.

### 7.5 Debugger  *(Decided)*
Debug Adapter Protocol over stdio (Content-Length framed JSON, parsed with json-glib). First adapter: gdb's built-in DAP (`gdb --interpreter=dap`, gdb 15.1 on the target), for C, C++ and Rust. A short spike confirms breakpoints, stepping, stack, scopes, variables and evaluate work reliably; if not, fall back to GDB/MI. Other adapters (`lldb-dap` and so on) can follow with the same client. UI: click the gutter to set a breakpoint; continue, step over, step into, step out; stack, locals and watch expressions; current-line marker. Not in the first version: attach, core dumps, remote debugging.

### 7.6 Agent  *(Decided; names Proposed)*
**Providers.** One agent loop, provider adapters behind an interface:
- OpenAI-compatible (Chat Completions with tool calls, streaming): covers OpenAI, local llama.cpp and other compatible servers, and Gemini through Google's OpenAI-compatible endpoint.
- Native Anthropic Messages adapter.
- Native Gemini adapter later, only if the compatible endpoint lacks something needed.
Provider APIs change; each adapter is verified against current documentation when it is planned. Streaming uses server-sent events; a Cancel control stops the request and the running tool.

**Setup dialog.** Opened the first time the Agent tab is used: provider, base URL, API key (stored in the system keyring, environment variable as fallback; JSON only ever holds a reference such as `keyring:name` or `env:NAME`, and a plaintext key is rejected), model, a connection test, the permission preset and the context budget. If a llama.cpp server answers on `127.0.0.1:8080`, the model and context size are read from `/props` and the monitor (7.7) is offered.

**Tools** (one registry, also exposed over MCP): `read_file` (line ranges), `replace_text` (exact match), `create_file`, `list_dir`, `search`, `open_file`, `run_build`, `run_command`. Tool results are truncated with an explicit marker.

**Context.** Each request carries the system prompt, the project file (7.8) and a small editor-state block (open tabs, active file, selection, folder root); everything else comes through tools. Per-endpoint context budget; at about 80% older turns are summarised. Per-task limits: 50 turns and a token cap by default.

**Permission presets.** Rules are JSON, a glob per tool, the last matching rule wins:
| Preset | Edits | Shell | Network |
|---|---|---|---|
| Restricted (default) | ask each time | ask each time | model endpoints only |
| Balanced | free inside the folder | free, with a deny list: commit, push, destructive git, writes or deletes outside the folder, other network | model endpoints only |
| Unrestricted | free | free | free |
| Custom | user JSON | user JSON | user JSON |
Switching to Unrestricted needs a confirmation dialog and shows a persistent red badge. **Hard guards in every preset:** the agent cannot change its own preset and cannot read API keys. Approvals in Restricted mode are inline Approve and Deny buttons with "allow for this session". A default "never send to remote endpoints" list (`.env`, `*.pem`, `id_*`, `*secret*`) is enforced in the file tools. The status bar shows the active endpoint and whether it is local (loopback) or remote; an append-only request log in the XDG state directory records endpoint, time, sizes and tool calls (prompt text off by default).

**Chat.** `/clear` saves the transcript to the archive and starts empty. Archives are JSON plus an exported Markdown copy under the XDG data directory, per folder; `/history` lists and opens them read-only. Every message, code block and generated prompt has a **Copy** button; `/copy` copies the last output.

**Slash commands and the gate framework.** Commands are Markdown templates in `.vermilion/commands/` (user-editable) with built-in defaults. `/gate <task>` fills the standard gate prompt (opening block, CONTEXT, STRICT RAILS, STEPS, EVIDENCE) from the project file and live facts such as `git status` and the modified-file list, and shows it as a card with one Copy prompt button. `/audit` takes a pasted agent report, runs the checks with the tools and issues a verdict (PASS, PASS WITH NOTES, FAIL) and the next prompt. Scheduled after the agent core (v0.4).

**Review of agent changes: side-by-side diff.** The core diff engine returns aligned rows (left, right, kind, word-level highlights). The `DiffView` widget is two read-only source views with filler lines and a shared scroll adjustment, folding of unchanged regions and Accept or Revert buttons per hunk in the centre. It serves proposed edits in Restricted mode, after-the-fact review in the other presets, the "show diff" button on external changes, and audits. Each agent turn is a change set; files changed on disk that are not open are snapshotted into the XDG cache first so revert never depends on git. One undo step per agent turn per document; edits carry the buffer version the agent read and fail if the buffer changed meanwhile.

**External changes.** `GFileMonitor` on open files and expanded folders (not the whole tree), debounced; our own saves are ignored by comparing modification times. A clean buffer changed on disk reloads automatically as one undo step; a dirty buffer shows a banner: Reload, Keep mine, Show diff. Agent edits to an open file go through the buffer; edits to closed files go to disk after a snapshot.

**Cost.** The user defines input and output price per million tokens for each endpoint, including a local model, to see what the same work would cost on a paid model. No prices are shipped. Token counts come from the `usage` fields of each response (streaming requires usage on the final chunk, confirmed per provider). Totals per conversation and per session sit beside the context bar.

**MCP.** Vermilion is an MCP server over stdio exposing the same tool registry, and an MCP client for stdio tool servers. HTTP transports and ACP are deferred.

### 7.7 ctxmon (model monitor)  *(Decided)*
From 2026-11-07 Vermilion's ctxmon is the source of truth and all ctxmon development happens in this repository (Adam). The Python original `reference/ctxmon/ctxmon_gtk.py` (Adam's own code, GPL-3; copied from `~/Projects/llm-suite-test` on 2026-11-05, sha256 `895a34f7...d407`, with its logo `ctxmon.svg`) is frozen: it is never built or linked, it is not re-copied, and it serves only as the oracle for the M1 tests and as the behavioural reference for the M2 cards. Gate M2.11 removes all Python from the repository (Adam, 2026-11-07): the expected values become inline C++ tables in the tests, the original and `tools/ctxmon_oracle.py` are deleted, and `SourceCheck.Repo` rejects any `.py` file from then on.

**Layout.** `src/ctxmon/core` holds the logic (no GTK), `src/ctxmon/ui` the cards and the standalone executable, `tests/ctxmon` the tests; core and UI never mix (section 4).

**Logic (M1).** Log token lines (`n_tokens`, `n_past`, session floor), timing lines and finished-request history, `/slots` and `/props` parsing, live speed over a two-second window, server state with history pruning, warnings at 80% and 95% and when the context would fill within a minute, graph geometry, `fmt_duration`, VRAM from AMD sysfs or `nvidia-smi` output. Everything is a pure function on text, JSON strings or a directory tree, tested against expected values produced by the Python original (oracle script and fixtures, M1.0). The Python's thread-based poller and blocking HTTP are not ported; the I/O is redesigned in M2 on GIO and libsoup async calls on the main loop.

**UI (M2).** A card per server (several servers supported), with graphs and the terminal, in two forms built from one shared widget set: the **CTXMon** tab in the right-hand stack (added when the stack exists, M3), and a **standalone executable** (`vermilion-ctxmon`, own `GtkApplication` window, no editor or folder needed; name and application ID proposed). Dropped from the original: the tray icon (GTK 4 has none) and `notify-send` (replaced by `GNotification`). llama.cpp servers only.

### 7.8 Project file  *(Decided)*
Two files in the opened folder: `PROJECT.md` (human and agent readable: purpose, directory layout, conventions, decisions, current state, next steps, in the shape of this document) and `.vermilion/project.json` (build, run and debug configurations, permission overrides, agent settings, gate templates in use) *(names proposed)*. New Project creates both from a wizard (name, language and build preset, skeleton); Open Folder offers "Adopt as project", where the agent audits the tree read-only and writes the first `PROJECT.md`. The agent proposes updates to `PROJECT.md` after changes as an ordinary change set.

## 8. Configuration and data locations  *(Proposed names)*
| What | Where |
|---|---|
| Application settings (key file) | `$XDG_CONFIG_HOME/vermilion/settings.ini` |
| Agent endpoints, presets, rules (JSON; keys by reference only) | `$XDG_CONFIG_HOME/vermilion/agent.json` |
| Per-folder project settings | `<folder>/.vermilion/project.json`, `<folder>/PROJECT.md` |
| Chat archive | `$XDG_DATA_HOME/vermilion/chats/<folder>/` |
| Snapshots for revert | `$XDG_CACHE_HOME/vermilion/snapshots/` |
| Request log | `$XDG_STATE_HOME/vermilion/requests.log` |
| API keys | system keyring (libsecret) |
GSettings is not used (it needs installed schemas). Nothing is written next to the executable.

## 9. Do not touch
- `opencode.json` and `~/.jetbrains/` (the coding agent's harness).
- `res/vedt/` (Vermilion's own artwork: icon, small icon, logo), except when Adam replaces an asset.
- `LICENSE` (GPL-3 text) and the copyright headers.
- `.idea/` and `cmake-build-*/` are per-user and never committed.

## 10. Open items
- Names of the configuration files and JSON schemas (decided when each is first written; config-format work is plan-first, AGENTS.md rule 10).
- Whether gdb's DAP is solid enough (spike at the debugger milestone).
- Provider details: usage-on-stream support, tool-call formats (checked at the provider gate).
- Packaging (a `.deb` or AppImage; Flatpak is excluded because its sandbox blocks the host compiler, debugger and the agent's shell).
- Agent shell confinement (Bubblewrap) as a later hardening step.
