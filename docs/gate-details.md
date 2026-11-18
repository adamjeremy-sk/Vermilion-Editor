# Gate details and notes: Vermilion Editor

The detail behind the gates of `docs/roadmap.md`. The roadmap keeps one short row per gate. Here are the longer descriptions of a gate, and the notes that the marks in the Done column point to. The line rule of the roadmap applies here too: no line has more than 60 words.

## M0: Reset and foundation

### Notes

\* Every gate committed and pushed; 67 tests. Milestone review: `docs/milestones/M0-review.md`.

## M1: Model monitor logic (core, tests from the Python oracle)

### Notes

\* Every gate committed and pushed; 117 tests. Milestone review: `docs/milestones/M1-review.md` (held 2026-11-08, before the M1.7 hardening gates; outcome appended).

† 103 tests.

‡ 109 tests.

¶ 117 tests.

## M2: Editor

### Gate details

- **M2.0** has two spikes.
  - (a) GtkSourceView with a 50 MB file: measure open time, scrolling and memory.
  - (b) Look spike: one bare window with a menu bar, a notebook with an editor, a status bar and the classic stylesheet in a light and a dark palette (ARCHITECTURE.md 7.10). It also checks how GTK 4.14 reports the system dark mode.
  - If the CSS fights GTK, fall back to the lighter touch: grey, flat buttons, tight spacing.
- **M2.1** builds the main window.
  - Menu bar (File, Edit, View, Help at first), no toolbar, and a status bar with Line/Column, INS, Language and Tab Width panels. Every command is a `GAction` shared by menu and shortcuts.
  - Document tabs: fixed width of 18 characters with an ellipsis, tooltip with the path, unsaved marker, middle-click close, drag to reorder, arrows and wheel on overflow. The close button sits at the tab's right edge (a bold `×` in a raised bevelled square of about 16 px). Tabs never stretch to fill the strip.
  - An untitled document at start (lowest free number). Closing an edited tab, or quitting with edited documents, asks Close Without Saving / Cancel (Save joined at M2.2).
  - Closing the active tab selects its left neighbour (right if it was first); closing a background tab keeps the active one. Closing the last document leaves a blank window with every document command greyed out (ARCHITECTURE.md 7.10).
  - Adam checks the look, including the fused selected tab and the sunken editor well (light palette only; dark and Follow system arrive with M2.5).
- **M2.1c** application icons.
  - Rename `res/vedt/Vermilion Editor Icon.svg` to `vermilion-icon.svg` (README reference updated, rule 8 proof).
  - Every build renders both SVGs to PNG with `rsvg-convert` (`librsvg2-bin`; configure stops with the package name if it is missing): `vedticon.svg` at 16 and 24 px, `vermilion-icon.svg` at 32, 48, 64, 128 and 256 px.
  - The PNGs are compiled into the resources under `icons/<n>x<n>/apps/`, named by the application ID; GtkApplication adds that path to the icon theme. GTK picks the small icon for the title bar and the large one for the panel, Alt+Tab and About. The resource test checks every size. Fully dictated.
- **M2.1d** continuous integration (build infrastructure, ahead of M2.2).
  - The workflow `.github/workflows/ci.yml` runs on every push and pull request to `main`.
  - A clean Ubuntu 24.04 runner installs the README's packages (without `clang-tidy`), configures, builds and runs the full `ctest`, including `SourceCheck.Repo` and `Format.Check`. UI tests that need a display will add `xvfb`.
- **M2.2 Open.**
  - Before a file loads, the core `TextScanner` reads it once in 64 KiB blocks ("Checking … P%") and decides the encoding: a strict whole-file UTF-8 check, a UTF-8 or UTF-16 byte-order mark, else Windows-1252, or Latin-1 when one of its five undefined bytes occurs.
  - It also decides the line endings (a file with several kinds shows "Mixed" and saves with the ending most lines use), the longest line, and whether the file is binary (a NUL byte).
  - GtkSourceView then loads with exactly that encoding and no language; the language is set after the load (M2.0a).
  - A binary file, or a line longer than 350,000 characters (Adam measured 500,000 and it still felt fine; a setting at M2.5), asks "Open Anyway / Cancel". A binary file opened anyway, and one whose bytes had to be escaped, are read-only.
  - Word wrap is off when a file opens, is switched on per document, and is never asked or saved as a default. The dialogs start in the home folder.
- **M2.2 Save.**
  - File → Save (Ctrl+S) and Save As… (Ctrl+Shift+S) go through `GtkSourceFileSaver`: a new copy beside the original, flushed and swapped in, so a failure leaves the original; no backup files.
  - The implicit trailing newline is off so a file round-trips byte for byte; the byte-order mark stays in the text as U+FEFF.
  - A file changed on disk asks "Save Anyway / Cancel". Save As refuses a path open in another tab. The document is locked while it loads or saves; errors keep it unsaved and give the reason.
- **M2.2 Questions and status bar menus.**
  - Closing an edited tab asks Save / Close Without Saving / Cancel (untitled: Save As first; cancelling it cancels the close). Quitting asks about each edited document in turn, selecting its tab; Cancel stops the quit.
  - The Encoding panel opens "Reopen as" (UTF-8, UTF-16 LE and BE, Windows-1252, Latin-1; with unsaved edits it asks Save and Reopen / Discard Changes and Reopen / Cancel) and "Save as" (the same plus UTF-8 with BOM).
  - The Line Ending panel converts to LF, CRLF or CR at the next save.
- **M2.2 Process.** Measured by Adam: a 38.9 MB file saves in 6.5 s with the window usable. First gate of the Qwen-writes-implementations trial: Qwen wrote the core `.cpp` files, `document_saver.cpp` and the status bar; the Orchestrator dictated headers, tests and the rest of the UI.
- **M2.2b** Help → Index.
  - Every `docs/manual/*.md` is collected by CMake into the resources (a new topic needs no code change) and shown in a separate read-only window (F1; a second Help → Index brings it forward) that opens on `index.md`. It replaces a separate Help → Keyboard Shortcuts item.
  - The window has labels and grids, links between topics that replace the page, and Back and Index buttons (Alt+Left) over a tested page history.
  - A small renderer handles a Markdown subset (headings, paragraphs with joined source lines, lists, bold, inline code, tables, links to topics; anything else shows as plain text), tested without a display.
  - A test checks that every manual file is in the program, linked from the index and inside the subset.
- **M2.2c** crash recovery (a basic editor function, not a new feature; talk held and decided the same night).
  - Every edited document, untitled ones included, is copied to a recovery file in `$XDG_STATE_HOME/vermilion/recovery/<program>/` (`light` for `vermilion`, `ide` for `vermilion-ide`). The copy is made 5 s after typing stops and at the latest 30 s after the first unsaved change. Saving or Close Without Saving deletes it.
  - A large document times its own main-loop freeze and the next copy waits at least 20 times that.
  - Above a ceiling size there is no copy and the status bar says so. The ceiling is counted in characters (fixed at 100,000,000 from Adam's run on the 39 MB file; the copy logs its hold, nothing is measured outside the repo).
  - Files and folder are private (0700 and 0600) and barred to the agent's file tools. The file format was planned with Adam first (AGENTS.md rule 10).
  - At start-up each leftover copy is offered in turn with the time of the copy and of the file: Recover, Discard or Decide Later (Recover All when more than one waits). The Compare button comes with the diff view (M2.6).
- **M2.5** settings.
  - A settings window (classic OK, Cancel, Apply) and key file: colour mode Light, Dark or Follow system with the two classic palettes, GtkSourceView schemes (`classic` and a dark one), editor font (default: system monospace), tab width; the Tools menu.
  - Also here (from the M2.3 design talks): a check box for the current-line band, a few style presets, and light and dark defaults for the editor scheme file that M2.3 already writes to the cache folder.
- **M2.5b** Help window (a Windows 98 Help screenshot is a layout idea only; our own look and branding, ARCHITECTURE.md rules stay).
  - The Help → Index window (M2.2b) gets two panes. Left: a **Contents** tab (a tree of manual topics and their sections) and a **Search** tab (a text box with Match case, Whole word and Regex, and a list of matching pages with a snippet). Right: the page view.
  - Text-only buttons **Hide** (collapses the left pane), **Back** and **Forward**. No Index tab (Search covers it; it would need a hand-made keyword list). The page text becomes selectable.
  - Search reuses the core `SearchOptions`, `effective_search_text` and `pattern_error` with GLib's regex over the page text (the editor's `SearchSession` is bound to a text buffer and is not reused). Forward extends `PageHistory`.
  - Placed before M2 ends by the Orchestrator; scope to be confirmed with Adam in a short design talk first.
- **M2.6** diff (the diff is part of the light editor `vermilion` too).
  - **Engine** in core: Myers on lines behind one function, so another algorithm (histogram, in `docs/ideas.md`) can replace it; aligned rows; word-level highlights inside paired lines.
  - **`DiffView` widget** in the editor base: side by side with filler lines, shared scroll, folding of unchanged regions.
  - Wired in here: the Show diff button of the external-change banner (M2.4) and the Compare button of the crash-recovery question (M2.2c). Other entry points (for example comparing a document with its file on disk) and the view's details are settled in a design talk first.
  - The agent's Review and the per-hunk controls (M5.6b, M5.11b) use it later.
- **M2.7c** Open Recent: a menu of the files opened last, with a way to clear it. A design talk with Adam comes first: how many entries, where the list is kept, what happens to a file that no longer exists, and whether it follows the start state of M2.7.
- **M2.7b** the light program.
  - The M2 code moves into a static library and `vermilion` becomes a thin `main` over it (no `--feather` flag).
  - `vermilion [file...]`: files from the command line and from "Open with", its own application ID, a desktop entry for text files, the single-instance rule, quits when the last document closes. It shows only File, Edit, Search, View and Help, treats the "blank window" start state as untitled, and uses recovery folder `light`.
  - A ctest checks that the executable links no VTE, libsoup, libsecret, agent or ctxmon code. Startup time is measured.
  - The command-line options are documented in a new manual topic, Command-line options, where every later flag goes too.

### Notes

\* 117 tests; results in ARCHITECTURE.md 7.1 and 7.10.

† 131 tests. Five steps in one prompt, then STEP 4b (fixes after Adam tried the tabs) and STEP 5 (menu colours); fix 1 shortened Help → About.

‡ 132 tests. Two steps (the rename, then rendering and use). Fix 1: GTK focuses the program name when About opens, so the stylesheet now draws selected text in About without a highlight.
On XFCE with Greybird the title bar shows the window-menu arrow, not an application icon, so the badge is used where XFCE asks for small icons.

¶ 132 tests. One step; the workflow was checked as YAML before it was issued and is first run by GitHub after the push.

§ 205 tests. The core (scanner, formats, file documents, open messages), the open flow, and the save part (core, saver, status bar menus, Save, Save As, questions), then one fix (dialogs start in the home folder; Save and Reopen).
Adam's 18 hand checks of open and save matched byte for byte. Long-line limit 350,000 characters; saving 38.9 MB took 6.5 s.

‖ 259 tests. Six one-step prompts: the spans, the page blocks, the page history, the manual as a second resource, the help window, the wiring.
Qwen wrote four `.cpp` files from specifications; audits used a differential fuzz against the Orchestrator's reference code. The manual gained the topic "Using the manual".

\# 482 tests. Fifteen prompts; Qwen wrote the `.cpp` files from specs, and headers, tests and wiring were dictated. A recovered document takes over its old copy's id.
Adam's run on a 52,428,800-character file: a copy holds the main loop 95 to 125 ms, a restore of the whole text takes 976 ms, nothing felt slow. The ceiling stays at 100,000,000 characters and the text is taken in one call (ARCHITECTURE.md 7.11).

\*\* 715 tests (ARCHITECTURE.md 7.1 and 7.10). The Find, Replace and Go to Line windows, F3 and Shift+F3, the always-on current-line band, and Replace All as a cancellable sliced job that is one Undo step (STEPS 13a to 13f). Adam's hands-on list on the 50 MB file passed.
Measured: 289,088 matches replaced in 64.5 s; Undo of a 19,040-match run took 10.6 s with the window frozen (accepted; the manual documents the cost).
Known limit: highlighting every match on one huge line can freeze the window. The manual gained the topic "Find, Replace and Go to Line".

## M5: Agent core (v0.3.0)

### Gate details

- **M5.1** provider.
  - Fixes the schema of `agent.json`: endpoint, model, key reference `env:NAME`; a plaintext key is rejected. Plan first (AGENTS.md rule 10). Until M5.8 the endpoint is written by hand.
  - A spike inside the gate: does llama.cpp send `usage` on the last stream chunk, and does tool calling work with Qwen when the server runs with `--jinja`.
  - Also removes the unused `libsecret` requirement from `CMakeLists.txt`, the CI install line and the README package list.
- **M5.2** permission engine.
  - Presets Restricted, Balanced, Unrestricted and Custom; rules (a glob per tool, the last match wins); hard guards (the agent cannot change its own preset or read keys); the never-send list.
  - A per-preset "ask before saving" setting: on in Restricted, off in Balanced and Unrestricted. `run_command` is an always-ask tool that is off in Balanced.
  - It comes before the tools because they call it (the order was left to the Orchestrator).
- **M5.3a** the original eight tools became thirteen, in three gates.
  - Each tool is a name, a JSON schema and a function (the shape OpenAI tool calls use, so a later MCP adapter would not change the registry). Every call goes through the permission engine (M5.2). Results are truncated with an explicit marker.
  - `read_file`: a file or a folder; line ranges; a tail mode; size, line count, encoding and line ending.
  - `search`: regex, file globs, context lines, match counts, a file-name mode; it reads open buffers, so it sees pending agent edits.
  - `editor`: state only here (open buffers, active file, selection, cursor); its open and go-to-line actions come in M5.3b.
  - `git`: read-only (status, diff, log, show); commit, push and destructive git are not offered.
- **M5.3b** edit-side tools and staged edits.
  - Tools: `replace_text` (exact match; it carries the buffer version that `read_file` returned and fails if the buffer changed since), `create_file` (with an overwrite option), `file_ops` (delete, rename, copy, mkdir), `save_file`, and the open and go-to-line actions of `editor`.
  - Agent edits stay in the buffer, unsaved and undoable, until saved. An edit to a closed file opens it in a tab (there is no tab limit) and leaves it unsaved. `save_file` and the save that `run_build` makes follow the preset's "ask before saving" setting.
  - The plan settles what happens when the buffer already holds the user's unsaved changes (it asks first), and whether `replace_text` needs whitespace tolerance (OpenCode's `edit` tries several matching strategies; the expectation is that Qwen copes with exact match). Decided from the OpenCode usage log (HANDOFF.md, Backlog) and at DOGFOOD 4; `replace_lines` stays in `docs/ideas.md` until exact-match failures show.
- **M5.3c** build-side tools.
  - `run_build` refuses to compile while open buffers have unsaved changes. Restricted asks "Save N files and build?" once per cycle; Balanced and Unrestricted save the agent-touched files themselves unless "ask before saving" is on. It returns the problems parsed by the M4.4 matchers (file, line, severity, message) and always the warning count.
  - `run_tests`: a filter; passed, failed and skipped counts, where 0 tests found is a failure; warnings from the build and the test output (compiler warnings, `GLib-CRITICAL` and `GLib-GObject-WARNING` messages, sanitizer reports); "passed with warnings" is its own status.
  - `format_file`: clang-format with the project's `.clang-format`.
  - `run_command`, the always-ask fallback: it removes every variable named in `agent.json` as a key reference from the shell's environment, sees only the disk, and its changes are not tracked.
- **M5.6** change sets, lean.
  - Each agent turn is a change set kept in memory for the session. Before the first edit of a file in a turn it is snapshotted into `$XDG_CACHE_HOME/vermilion/snapshots/` (an open buffer: its text; a closed file: its bytes), so revert never depends on git.
  - Whole-turn revert: a file the user changed after the agent is asked about first; a file the agent created is removed; a file the agent deleted or renamed is restored.
  - Deletes, renames and copies made through `file_ops` are tracked (the bytes are snapshotted first). Changes made by `run_command` are not, and the manual says so.
  - Not included: persisted change sets and one undo step per turn.
- **M5.6b** reverse hunks in core.
  - A change set also keeps the agent's result and exposes its hunks (the diff of the snapshot against the result, from the M2.6 engine).
  - Reverting a hunk finds the agent's lines in the current text by exact context and puts the snapshot's lines back. It refuses with a plain message when the text there has changed since (no fuzzy matching). Accepting a hunk keeps it.
  - The plan settles what Accept writes to disk and when (per hunk, or once every hunk of a file is decided).
  - Edge cases: adjacent hunks, line shifts after an earlier revert, the end of the file, a missing final newline, CRLF, repeated lines.
- **M5.7b** pending-changes bar.
  - While a change set has unsaved or undecided files, the bar reads "N files changed: Review, Accept all, Revert all". Accept saves, Revert restores from the snapshots, Review opens the DiffView of M2.6. "Allow for this session" covers saves.
  - **◆ DOGFOOD 4:** the first try of the built-in agent against local llama.cpp on small tasks, with staged edits. CLion/OpenCode remains the primary agent until Adam judges it good enough (set 2026-11-13).
- **M5.8** setup dialog.
  - Provider type (OpenAI-compatible), base URL, the name of the environment variable that holds the key (Vermilion stores no key), model, connection test, permission preset.
  - It reads `/props` when llama.cpp answers on `127.0.0.1:8080`; the offer of the ctxmon tab joins when that tab exists (M6.5).
  - The key resolver knows `env:`; a `keyring:` reference gets "not supported in this version".
- **M5.12** request log and privacy indicator.
  - The status bar shows the endpoint as local (loopback), network (a private IP address) or remote (anything else). A hostname other than `localhost` counts as remote, because resolving it would block the main loop.
  - An append-only `requests.log` in the XDG state folder: endpoint, time, sizes, token counts, tool names; no prompt text by default, never a key.
