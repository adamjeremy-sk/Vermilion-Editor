<p align="center">
  <img src="res/vedt/Vermilion%20Editor%20Icon.svg" width="160" alt="Vermilion Editor icon">
</p>

<h1 align="center">Vermilion Editor</h1>

<p align="center">
  A native Linux code editor with a built-in AI agent, build and debug support, and a live model monitor.<br>
  C++17 &middot; GTK 4 &middot; GtkSourceView 5 &middot; GPL-3.0-or-later
</p>

> **Status: pre-alpha. Nothing runs yet.**
> Vermilion Editor is being written from scratch. At this stage the repository holds the plan: the architecture, the
> roadmap and the rules for the development process. The first code (a window skeleton) arrives with roadmap gate M0.2.
> Everything under "Planned" is a plan, not a feature you can use. See [Status](#status).

---

## Contents
1. [What Vermilion Editor is](#what-vermilion-editor-is)
2. [Status](#status)
3. [The window](#the-window)
4. [Planned features](#planned-features)
5. [The AI agent](#the-ai-agent)
6. [CTXMon (model monitor)](#ctxmon-model-monitor)
7. [Project file](#project-file)
8. [Configuration and data](#configuration-and-data)
9. [Building](#building)
10. [Repository layout](#repository-layout)
11. [How the project is developed](#how-the-project-is-developed)
12. [Roadmap](#roadmap)
13. [Feedback](#feedback)
14. [Licence](#licence)

---

## What Vermilion Editor is
A text and source-code editor for Linux (Ubuntu 24.04 is the reference system), written in C++17 on GTK 4 and
GtkSourceView 5. It is aimed at people who write code with an AI agent next to them and want one native window for the
whole loop: edit, ask the agent, review what it changed, build, run, debug and watch the model that is serving it.

Principles:
- **Native and direct.** GTK 4 and GLib through their plain C APIs; no language bindings, no vendor SDKs, nothing that only
  wraps another API. The agent speaks the providers' HTTP protocols itself; builds and debuggers run as subprocesses.
- **The agent is built in, not bolted on.** It works through the editor's own tools, under permission presets you control.
- **Reviewable.** Every change the agent makes is a change set you can read side by side and revert.
- **Local first.** A local llama.cpp server is a first-class endpoint, with a monitor for it; cloud providers use your own keys.
- **Linux only.** Wayland and X11 through GTK. English interface. No telemetry.

The project began as a port of the Windows editor CrimsonEditor and was restarted from scratch; nothing of that code remains.
The abandoned port is part of the git history and summarised in `docs/archive/`.

---

## Status

| Area | State |
|---|---|
| Editor, tree, build, terminal (v0.1) | Planned, not started |
| Agent (v0.2) | Designed, not started |
| Debugger (v0.3) | Designed, not started |
| CTXMon tab and standalone monitor, cost, extra providers, project file, gate framework, MCP (v0.4) | Designed, not started |
| Code | None yet. Next gate: M0.2, the CMake skeleton and an empty window. |
| Releases | None |

---

## The window
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
One window, one opened folder. The right-hand stack has three tabs: the directory sidebar (**Files**), the **Agent** and **CTXMon**, the live model monitor (which also exists as a standalone program). The bottom panel
has the build output, the problem list, terminals and the debugger views.

---

## Planned features

### Editor (v0.1)
- Tabs; open and save; encodings (UTF-8 by default, others through GLib) and line endings detected and preserved (LF for new files).
- Syntax highlighting, line numbers and colour schemes from GtkSourceView; follows the system dark mode.
- Search and replace with regular expressions, go to line.
- External changes: a clean buffer reloads automatically when the file changes on disk; a modified buffer shows a banner with
  Reload, Keep mine and Show diff.

### Directory sidebar (v0.1)
On the right-hand side. One tree of folders and files (folders first), lazy loading, a file filter, a hidden-files toggle
(off by default), a Sync button that follows the active tab, rename, delete (to trash), copy and move, drag and drop, and
automatic refresh. The behaviour is specified in [`docs/requirements/directory-sidebar.md`](docs/requirements/directory-sidebar.md).

### Build, Output and Problems (v0.1)
Per-folder build and run commands in `.vermilion/project.json`, with presets that detect CMake, Make, Ninja, Cargo, Gradle and
Bazel. Output streams into the Output pane; compiler errors from gcc, clang, rustc and javac become a Problems list and
clickable links to the source line.

### Terminal (v0.1)
Terminal tabs (VTE) in the bottom panel, started in the opened folder. Run and Debug use a terminal so the program has a real
stdin and stdout.

### Debugger (v0.3)
Debug Adapter Protocol, using gdb's built-in adapter first (C, C++, Rust). Click the gutter to set a breakpoint; continue, step
over, step into, step out; call stack, local variables and watch expressions; the current line is marked in the editor.
Not planned for the first version: attach, core dumps, remote debugging.

---

## The AI agent
Planned for v0.2 and later; the design is in [`ARCHITECTURE.md`](ARCHITECTURE.md) section 7.6.

**Providers.** OpenAI-compatible servers (which covers OpenAI, a local llama.cpp server and Gemini's compatible endpoint),
Anthropic's native Messages API, and a native Gemini adapter if the compatible one falls short. You bring your own keys.
Replies stream in and can be stopped at any time.

**Setup dialog.** The first time you open the Agent tab, a dialog takes care of everything: provider, base URL, API key (kept
in the system keyring), model, a connection test and the permission preset. If a llama.cpp server is running locally, it fills
in the model and context size and offers the CTXMon tab.

**Tools.** The agent works through the editor: read a file (line ranges), replace text, create a file, list a directory, search,
open a file in a tab, run the build, and run a shell command.

**Permission presets.**

| Preset | Edits | Shell | Network |
|---|---|---|---|
| **Restricted** (default) | ask each time | ask each time | model endpoints only |
| **Balanced** | free inside the folder | free, with a deny list (commit, push, destructive git, writes outside the folder) | model endpoints only |
| **Unrestricted** | free | free | free |
| **Custom** | your JSON rules | your JSON rules | your JSON rules |

Unrestricted needs a confirmation and shows a red badge. In every preset the agent cannot change its own preset and cannot
read your API keys. A default list of files that are never sent to a remote endpoint (`.env`, `*.pem`, `id_*`, `*secret*`)
is enforced, the status bar shows whether the active endpoint is local or remote, and a request log records what was sent
(not the prompt text, unless you turn that on).

**Reviewing what the agent did.** Every turn is a change set shown in a side-by-side diff with folding and per-hunk Accept or
Revert. One undo step undoes a whole agent turn. Files changed on disk are snapshotted first, so reverting does not depend
on git.

**Chat.** `/clear` saves the conversation to an archive and starts fresh; `/history` opens archived chats; `/copy` copies the
last output, and every message, code block and generated prompt has a Copy button. Slash commands are Markdown templates in
`.vermilion/commands/`.

**Gate framework.** `/gate <task>` builds a self-contained work prompt (context, rails, steps, evidence) from your project file
and the live state of the tree and shows it with a one-click Copy; `/audit` takes the report that comes back, checks it with the
agent's tools and gives a verdict. The templates are yours to edit.

**Cost.** Enter input and output prices per million tokens for any endpoint, including your local model, to see what the same
work would cost on a paid model. No prices are shipped.

**MCP.** Vermilion will be an MCP server (stdio) exposing its tools to other agents, and an MCP client for outside tool servers.

---

## CTXMon (model monitor)
CTXMon is a live monitor for a running llama.cpp server, a native C++ port of Adam's `ctxmon` (the Python original is kept for
reference in [`reference/ctxmon/`](reference/ctxmon/)). It comes in two forms that share the same logic and the same cards:

- **Built in:** the **CTXMon** tab in the right-hand stack, next to Files and Agent, so the server your agent is using is one click away.
- **Standalone:** a separate program, `vermilion-ctxmon` *(name proposed)*, with its own window. It needs no editor and no folder, so
  you can watch a server while you work in any other tool. It is built from the same source tree.

What a card shows (one per server, several servers side by side):

- **Context:** percentage and bar, with tokens used, total and free. It says whether the number is exact (the request finished and
  the server log confirmed it) or an estimate (a request is still running).
- **Speed:** generation speed (live while generating, otherwise the last request), prompt-processing speed, and the time until the
  context is full.
- **Graphs:** context over time and speed over time.
- **Recent requests:** the newest finished requests with prompt and generated tokens, speeds and the context size afterwards.
- **Warnings:** at 80% and 95% of the context, and when the context would fill within a minute at the current speed, as a banner
  in the card and as a desktop notification.
- **GPU memory:** VRAM used and total (AMD through sysfs, or `nvidia-smi`).
- **Terminal:** shell tabs (VTE) so you can restart a server without leaving the window.

It polls each server's `/slots` and `/props` endpoints and reads the server log (start the server with `--log-file`) for token counts
and finished requests. It only talks to your own servers, and shows zeros when nothing is listening. Differences from the Python
original: GTK 4 has no tray icon, so there is none, and notifications go through `GNotification`. Settings are remembered between
runs; their file name is fixed when it is first written.

---

## Project file
A project has two files in its folder (names provisional): `PROJECT.md`, a readable description of the purpose, layout,
conventions, decisions and current state that the agent reads first and helps keep up to date, and `.vermilion/project.json`
with the build, run and debug configurations and per-project settings. **New Project** creates both from a wizard; **Open
Folder** can "Adopt as project", where the agent reads the tree and writes the first description.

---

## Configuration and data
Everything follows the XDG directories; nothing is written next to the executable.

| What | Where |
|---|---|
| Application settings | `~/.config/vermilion/settings.ini` |
| Agent endpoints, presets and rules (JSON) | `~/.config/vermilion/agent.json` |
| Per-project settings | `<folder>/.vermilion/project.json`, `<folder>/PROJECT.md` |
| Chat archive | `~/.local/share/vermilion/chats/` |
| Revert snapshots | `~/.cache/vermilion/snapshots/` |
| Request log | `~/.local/state/vermilion/requests.log` |
| API keys | system keyring |

File names are provisional until the gates that create them. Settings can be changed in the application or by editing the files.

---

## Building
Reference system: Ubuntu 24.04. Install the development packages:

```bash
sudo apt install build-essential cmake pkg-config libgtk-4-dev libgtksourceview-5-dev libjson-glib-dev \
  libsoup-3.0-dev libsecret-1-dev libvte-2.91-gtk4-dev libgtest-dev clang-format clang-tidy
```

Then:

```bash
git clone https://github.com/adamjeremy-sk/Vermilion-Editor.git
cd Vermilion-Editor
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The project is developed in CLion; opening the folder picks up `CMakeLists.txt`. `build/`, `cmake-build-*/` and `.idea/` are
ignored by git. There is nothing to install and no release yet.

Vermilion logs through GLib under the log domain `vermilion`, with secrets masked before anything is written. As in any GLib
program, debug and info messages are hidden by default; run with `G_MESSAGES_DEBUG=vermilion` (or `all`) to see them.

---

## Repository layout

| Path | Contents |
|---|---|
| `src/core/` | UI-free logic: diff engine, permission engine, build presets and problem matchers, the debug-protocol client, monitor logic, slash commands, cost, archive. No GTK headers. *(created at M0.2)* |
| `src/agent/` | Provider adapters, the agent loop, the tool registry, MCP. No GTK headers. *(M0.2)* |
| `src/ui/` | The GTK application. *(M0.2)* |
| `tests/` | GoogleTest tests for core and agent. *(M0.2)* |
| `res/vedt/` | Vermilion's own artwork: application icon, small icon, logo. |
| `reference/ctxmon/` | The Python original of CTXMon (`ctxmon_gtk.py`) and its logo (`ctxmon.svg`). Read-only reference for the port; not built. |
| `docs/roadmap.md` | The milestone and gate plan. |
| `docs/requirements/` | Behaviour specifications, for example the directory sidebar. |
| `docs/archive/` | Notes on the abandoned CrimsonEditor port. |
| `ARCHITECTURE.md` | The decided architecture. |
| `AGENTS.md`, `AI_ONBOARDING.md`, `HANDOFF.md` | The development process: rules for the coding agent, how the process works, and where it stands. |
| `opencode.json` | Configuration of the local coding agent's model and permissions. Do not edit casually. |

---

## How the project is developed
Vermilion Editor is developed with AI assistance in a structured way, and the structure is part of the repository.

- **Adam**, the owner, decides, builds, tests and commits. The coding agent never commits or pushes.
- **An orchestrator** (Claude, working in a claude.ai Project) plans the work, writes self-contained prompts, audits every
  result against the real tree with fresh commands, and maintains the documentation.
- **A local coding agent** (Qwen3.8-27B served by llama.cpp, driven through OpenCode) makes the code changes, one logical step
  per session, following [`AGENTS.md`](AGENTS.md).

Work proceeds in **gates**: small steps, each ending with verbatim evidence and an audit verdict (pass, pass with notes, or fail)
before the next one starts. Every gate that adds code adds its tests. The agent features of Vermilion itself will standardise
this process (`/gate` and `/audit`).

---

## Roadmap
The plan is in [`docs/roadmap.md`](docs/roadmap.md):

| Version | Contents |
|---|---|
| v0.1 | Editor, right-hand directory sidebar, build and Output pane, Problems, terminal |
| v0.2 | Agent core: providers, tools, permission presets, setup dialog, chat, archive, side-by-side diff |
| v0.3 | Debugger |
| v0.4 | CTXMon tab and standalone monitor, cost display, Anthropic and Gemini adapters, project file, gate framework, MCP |
| v1.0 | Packaging, documentation, notices |

---

## Feedback
Bug reports, ideas and questions are welcome as [GitHub issues](https://github.com/adamjeremy-sk/Vermilion-Editor/issues).
The project is at its very beginning and not yet organised for outside code contributions.

---

## Licence
Vermilion Editor is free software under the **GNU General Public License, version 3 or (at your option) any later version**
(GPL-3.0-or-later). See [`LICENSE`](LICENSE).

Copyright (C) 2026 Adam Hainstock (@adamjeremy-sk)

Every source file carries an SPDX licence header. The model monitor is a port of ctxmon from
[llm-suite-test](https://github.com/adamjeremy-sk/llm-suite-test), also by the author. The libraries used (GTK, GtkSourceView,
GLib, json-glib, libsoup, libsecret, VTE) are LGPL-licensed; GoogleTest is BSD-licensed. A full third-party notice file will be added before
the first release.
