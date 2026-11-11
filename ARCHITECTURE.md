# ARCHITECTURE.md: Vermilion Editor
**Status**: rewritten 2026-11-05 for the greenfield plan; section 7.10 (window, look, feather mode) added 2026-11-10. Sections marked *Decided* are Adam's decisions; *Proposed* sections are the Orchestrator's recommendations awaiting Adam; names marked *(name proposed)* are placeholders fixed at the gate that creates them. The project is written from scratch.

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
| Application ID | `io.github.adamjeremy_sk.Vermilion` (feather mode: `io.github.adamjeremy_sk.Vermilion.Feather`, proposed) |
| Look and feel | One fixed classic Windows 2000 style from a single GTK CSS stylesheet with a light and a dark palette (Light, Dark, Follow system); system fonts, GTK's own title bar, no icons, no toolbar (keyboard first: every command is a menu item showing its shortcut), no custom drawing code. Details: 7.10 (Adam, 2026-11-10). |
| Feather mode | `vermilion --feather`: the same program as a plain text editor with its own application ID. Details: 7.10 (Adam, 2026-11-10). |
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
| Output | Problems | Terminal | Debug                  | Agent: chat, diff;     |
|   build log, clickable errors                        | CTXMon: server monitor |
+------------------------------------------------------+------------------------+
```
One window, one opened folder at a time. Right-hand stack: **Files** (directory sidebar), **Agent** and **CTXMon** (model monitor) tabs. Bottom panel: Output, Problems, Terminal (VTE, several tabs), Debug (stack, variables, breakpoints, watches). Above everything sits a menu bar (no toolbar); below everything a status bar (7.10).

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
Tabs (`GtkNotebook`), open and save, encodings through GLib converters (UTF-8 default), line endings detected and preserved (LF for new files), search and replace with `GRegex`, go to line, line numbers, GtkSourceView language detection and style schemes (follows the system dark mode). Gutter marks for breakpoints and the current debug line. File monitoring (see 7.6 "External changes"). Large-file behaviour was measured in the M2.0a spike (GTK 4.14.5, GtkSourceView 5.12.0, async `GtkSourceFileLoader`; the window stays usable while a file loads): a 50 MB file of 0.78 to 1.0 million lines loads in 4 to 6 seconds with a peak of 0.5 to 0.7 GB when no language is set, but in 72 seconds (highlighting off) to 87 seconds (on) with a peak of 1.1 to 1.6 GB when the C++ language is set; a 2 MB file on one line takes 40 seconds and 5 GB and hangs the view. Setting the language only after the load has finished removes the slow load (the same C++ file: 4.4 seconds, the language set in 7 ms, colouring then fills in while the window stays usable, peak 1.1 GB), so the editor loads without a language and sets it afterwards. Word wrap on the 50 MB text file loads in 4.2 seconds but lags while it lays out and peaks at 6.8 GB. Select All and deleting the whole text stayed responsive in every run. The guards for very long lines and for word wrap on large files are decided with Adam at M2.2. Window furniture, document tabs, start state, status bar and shortcuts: 7.10.

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

**Chat UI details (Adam, 2026-11-10):** while the agent works, its status reads "Processing" (no playful or rotating words); the model's reasoning text is collapsed by default and expands to the full text, never cut short.

**Manual (Adam, 2026-11-11):** `docs/manual/` is the user manual, one short Markdown file per topic, listed in `docs/manual/index.md`. From M2.2b the topics are compiled in as resources and Help → Index shows them in a read-only window (a small renderer for a Markdown subset, no new dependency).

### 7.7 ctxmon (model monitor)  *(Decided)*
From 2026-11-07 Vermilion's ctxmon is the source of truth and all ctxmon development happens in this repository (Adam). The Python original `reference/ctxmon/ctxmon_gtk.py` (Adam's own code, GPL-3; copied from `~/Projects/llm-suite-test` on 2026-11-05, sha256 `895a34f7...d407`, with its logo `ctxmon.svg`) is frozen: it is never built or linked, it is not re-copied, and it serves only as the oracle for the M1 tests and as the behavioural reference for the M2 cards. Gate M2.11 removes all Python from the repository (Adam, 2026-11-07): the expected values become inline C++ tables in the tests, the original and `tools/ctxmon_oracle.py` are deleted, and `SourceCheck.Repo` rejects any `.py` file from then on.

**Layout.** `src/ctxmon/core` holds the logic (no GTK), `src/ctxmon/ui` the cards and the standalone executable, `tests/ctxmon` the tests; core and UI never mix (section 4).

**Logic (M1).** Log token lines (`n_tokens`, `n_past`, session floor), timing lines and finished-request history, `/slots` and `/props` parsing, live speed over a two-second window, server state with history pruning, warnings at 80% and 95% (no time-until-full estimate: dropped at M1.7 because it jumped with the speed figure, Adam 2026-11-08), graph geometry, `fmt_duration`, VRAM from AMD sysfs or `nvidia-smi` output. Everything is a pure function on text, JSON strings or a directory tree, tested against expected values produced by the Python original (oracle script and fixtures, M1.0). Hardened at M1.7 against hostile input: a server response over 1 MiB or nested deeper than 32 levels is not parsed (own depth scan before json-glib, which in version 1.8 crashes on very deep nesting), negative counts from `/slots` count as missing, and GPU files are read only if they are ordinary files of at most 64 bytes. The Python's thread-based poller and blocking HTTP are not ported; the I/O is redesigned in M2 on GIO and libsoup async calls on the main loop.

**UI (M2).** A card per server (several servers supported), with graphs and the terminal, in two forms built from one shared widget set: the **CTXMon** tab in the right-hand stack (added when the stack exists, M3), and a **standalone executable** (`vermilion-ctxmon`, own `GtkApplication` window, no editor or folder needed; name and application ID proposed). Dropped from the original: the tray icon (GTK 4 has none) and `notify-send` (replaced by `GNotification`). llama.cpp servers only.

### 7.8 Project file  *(Decided)*
Two files in the opened folder: `PROJECT.md` (human and agent readable: purpose, directory layout, conventions, decisions, current state, next steps, in the shape of this document) and `.vermilion/project.json` (build, run and debug configurations, permission overrides, agent settings, gate templates in use) *(names proposed)*. New Project creates both from a wizard (name, language and build preset, skeleton); Open Folder offers "Adopt as project", where the agent audits the tree read-only and writes the first `PROJECT.md`. The agent proposes updates to `PROJECT.md` after changes as an ordinary change set.

### 7.10 Window, look and feather mode  *(Decided 2026-11-10, Adam; names Proposed)*
**Look.** One fixed classic Windows 2000 style, made by a single GTK CSS stylesheet (a resource) loaded at application priority over the desktop's own GTK theme (the M2.0b spike tried both the desktop theme and GTK's built-in Default underneath; Adam chose the desktop base, which looked best; Vermilion never changes the theme name). No custom drawing code and no replacement widgets. Restyled: buttons, entries, notebook tabs, the menu bar, status bar panels, panel borders, paned dividers and selection colours. Left as GTK draws them: scrollbars, dialogs and the file chooser. Popup menus keep the desktop theme except their text colours (black, grey when disabled), the navy highlight with white text, and a gap before the shortcut (Adam, 2026-11-11). The structure (bevels as stacked inset shadows, tight spacing, no transitions, no rounded corners) is shared; the colours come from two palettes. **Light**: chassis `#CECECE`, white highlight edges, `#808080` and near-black shadow edges, white text areas, navy selection. **Dark**: the same structure on a dark grey chassis with a lighter highlight edge and a darker shadow edge. Colour mode is a setting: Light, Dark or Follow system. GtkSourceView uses its `classic` scheme in light mode and a dark scheme in dark mode. Follow system (measured in the M2.0b spike on XFCE with Greybird, GTK 4.14.5) reads, in order: the settings portal's `org.freedesktop.appearance` `color-scheme` (absent on Adam's XFCE: its portal has no Settings interface), a theme name ending in `-dark` (what XFCE changes; the spike followed Greybird-dark and back live through `notify::gtk-theme-name`), then `gtk-application-prefer-dark-theme` (never set by XFCE). In dark mode Vermilion sets prefer-dark for its own process so the native parts follow. A GtkSettings value is set or reset only when Vermilion's wanted state changes: touching it otherwise makes GTK announce the setting again, which looped in the spike. The full bevelled stylesheet worked, so the lighter-touch fallback is not needed. GtkSourceView 5.12 ships `classic` and `classic-dark`. Greybird's own GTK 4 stylesheet still prints about 50 parser warnings at every theme load; they come from the desktop theme, not from Vermilion. This is one fixed look with two palettes, not the dropped theme system. **Classic details (Adam, 2026-11-10, after a review of the M2.0b screenshot):** (1) the selected notebook tab is fused into its page: taller than the others, no border under it, the unselected tabs sit lower and darker (Win2000 tab control), in our palette, not the `#D4D0C8` family; if GTK 4's CSS rejects the negative margin that overlap would need, the fusion is made with matching colours and no border instead; (2) the editor sits in a sunken two-pixel well like the entries; styling the line-number gutter needs the GtkSourceView style scheme, not CSS, so it is decided after the well is seen; (3) the per-tab close button shows a bold `×` (text, not an icon) in a small raised bevelled square of about 16 px; (4) menu mnemonics stay hidden until Alt is pressed (Windows 2000's own default); no Pango underline markup in menu labels, which would not match the real access keys.
**Fonts, title bar, icons.** The system font for the interface (nothing bundled; W95FA was considered and declined), GTK's default title bar (no custom title bar), and no icons or artwork: the few buttons (status bar panels, dialog buttons) are text. The application icon is the one exception (Adam, 2026-11-11): `res/vedt/vermilion-icon.svg` (renamed from `Vermilion Editor Icon.svg`) above the name in Help → About and as the desktop icon; `res/vedt/vedticon.svg` as the small title-bar icon. Both are compiled into the resources and found through the icon theme by the application ID (gate M2.1c); installing them for the desktop belongs to packaging (M8) and the desktop entries (M2.6b). The editor font is the system monospace by default and a setting.
**Commands.** Every command is a `GAction`. Menu items, shortcuts and the few buttons point at the same action, so enabling and disabling (no document open: Save and Find greyed out) happens in one place. A menu or button is added only at the milestone that builds its feature; there are no dead items.
**Menu bar** (`GtkPopoverMenuBar`, underlined access keys): **File** (New File, Open File, Open Folder, Recent Folders, Save, Save As, Save All, Reopen with Encoding, Reload from Disk, Close Tab, Close Folder, Quit; New Project and Adopt as Project at M7); **Edit** (Undo, Redo, Cut, Copy, Paste, Select All, Line Endings); **Search** (Find, Find Next, Find Previous, Replace, Go to Line); **View** (Next Tab, Previous Tab, Line Numbers, Word Wrap, Zoom In, Zoom Out, Reset Zoom, Colour Mode; later Show Files, Agent, CTXMon, Bottom Panel, Output, Problems, Terminal, Debug); **Build** (M4: Build, Run, Stop, Next and Previous Problem, Build Settings); **Debug** (M6: Start Debugging, Continue, Step Over, Step Into, Step Out, Stop, Toggle Breakpoint, Remove All Breakpoints, Add Watch); **Agent** (M5: New Chat, Chat History, Permission Preset, Cancel Request, Agent Setup); **Tools** (Settings from M2.5; Project Settings, Open PROJECT.md, Open CTXMon Window later); **Help** (Index from M2.2b: the manual by topic, which replaces a separate Keyboard Shortcuts item; About). Open Folder, Recent Folders and Close Folder arrive with the sidebar at M3.0.
**No toolbar** (Adam, 2026-11-10, after seeing it in the M2.0b spike): the window is keyboard first and lean. Every command is a menu item that shows its shortcut, including Build, Run and Stop (M4) and the debugger's run control (M6); there is no button strip, not even during a debug session.
**Status bar** at the bottom of the window, as in gedit: a wide message panel on the left, then right-aligned recessed panels: Language, Tab Width, Encoding, Line Ending (each a flat button that opens a short list), Line and Column, INS or OVR. Later: the agent endpoint with its local or remote marker, the red Unrestricted badge, build status.
**Document tabs.** Fixed width, 18 characters of the system font (never resized to fit), long names cut with an ellipsis, the full path as tooltip, a close button at the tab's right edge (the name takes the remaining width; Adam, M2.0b), a marker for unsaved changes, middle-click closes, drag to reorder. When tabs overflow the strip, the notebook's arrow buttons and the mouse wheel move through them and the active tab scrolls into view (no scrollbar: that would need a custom tab strip). **No tab limit** (Adam, 2026-11-10, before M2.1): Vermilion never closes a tab on its own; an automatic limit is parked in `docs/ideas.md`. **Tab Width** (indent size, in the status bar) is per document, starting from the settings value.
**Start state and closing.** Default: a new untitled document. Settings can change it to reopen the last document (a single file, a small exception to the dropped session restore) or a blank window. Closing an untitled, unedited document needs no prompt; an edited one asks Save / Don't Save / Cancel (until saving exists at M2.2 the dialog offers Close Without Saving and Cancel; quitting with edited documents asks the same way). When the last document closes, feather mode quits and the full editor shows a blank window in which every document command (Close Tab, the Edit items, Next and Previous Tab; Save and Find once they exist) is greyed out. When the active tab closes, its neighbour to the left becomes active (the one to the right if it was first); closing a background tab, including with its close button, leaves the active tab as it is (Adam, 2026-11-10, after trying M2.1 STEP 4). Tabs keep their fixed width whatever their number; they are never stretched to fill the strip. Untitled documents take the lowest free number.
**Shortcuts** (fixed for now): `Ctrl+N` New, `Ctrl+O` Open, `Ctrl+S` Save, `Ctrl+Shift+S` Save As, `Ctrl+W` Close Tab, `Ctrl+Q` Quit; `Ctrl+Z` Undo, `Ctrl+Y` and `Ctrl+Shift+Z` Redo, `Ctrl+X`, `Ctrl+C`, `Ctrl+V`, `Ctrl+A`; `Ctrl+F` Find, `F3` and `Shift+F3` Next and Previous, `Ctrl+H` Replace, `Ctrl+G` Go to Line; `Ctrl+PageDown` and `Ctrl+PageUp` next and previous tab, `Ctrl+=`, `Ctrl+-`, `Ctrl+0` zoom. At the Build and Debug gates, Visual Studio keys: `F7` Build, `F5` Run or Continue, `F9` Toggle Breakpoint, `F10` Step Over, `F11` Step Into, `Shift+F11` Step Out.
**Settings window.** Classic and transactional: changes are held until `OK` (save and close), `Cancel` (discard) or `Apply` (save and stay open; disabled until something changes).
**Feather mode.** `vermilion --feather` starts the same program as a plain text editor, a replacement for gedit or Mousepad: files from the command line and from "Open with", a desktop entry for text files, no folder, project, agent, terminal or panels, menus File, Edit, Search, View and Help only. It registers its own application ID (`io.github.adamjeremy_sk.Vermilion.Feather`, proposed; read from the flag before the application registers; fixed for the process) so it runs independently of the full editor. Both modes read the same settings file (font, colours, tab width); only the start state differs, and feather treats "blank window" as an untitled document. The terminal, HTTP and agent libraries still load at startup, so startup time is measured at gate M2.6b; if it is too slow a separate program is reconsidered (the editor window class never depends on the panels, so that costs a split, not a rewrite).

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
- Large files: the guards for very long lines and for word wrap on large files (M2.2, from the M2.0a numbers in 7.1).
