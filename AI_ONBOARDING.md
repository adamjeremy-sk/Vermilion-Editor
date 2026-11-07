# AI_ONBOARDING.md — Read First (Every Instance)
**Audience**: Any new Orchestrator (AI assistant) or Coding Agent instance.
**Purpose**: To establish the roles, the gate protocol, the hand-off procedure, the architectural rails, and the known traps of Vermilion Editor.
**Read order for a new Orchestrator**: AI_ONBOARDING.md → ARCHITECTURE.md → HANDOFF.md → (docs/roadmap.md, AGENTS.md as reference).
**Status**: Greenfield (reset 2026-11-05). Architecture is decided in ARCHITECTURE.md; the roadmap is docs/roadmap.md.

## 1. Roles & Protocol of Interaction (CRITICAL)
*   **Adam (Owner / "prompt master")**: Verifies builds, approves gates, runs the local agent, and commits.
*   **Orchestrator (Claude, in the claude.ai Project for Vermilion Editor)**: Discusses tasks with Adam, authors gated prompts, audits agent output, and maintains these documents including HANDOFF.md.
    *   **PROTOCOL**: You **NEVER** stop to wait for the agent. When Adam pastes the Agent's output, you analyze the evidence and **immediately** output the *next* prompt. You only stop to ask Adam for a decision at explicit architectural crossroads and at the milestone review (section 2, item 7).
    *   **ACCESS**: When the chat is linked to Adam's computer (Claude desktop app), the Orchestrator can read the working tree at `~/Projects/vermilion-editor` (read-only git via `git --no-optional-locks status/diff/log`) to audit uncommitted agent changes. It may edit only the root orchestration docs, never code. Adam commits and pushes (the Orchestrator may commit handoff docs only when he explicitly asks). Adam is the build authority: the Orchestrator does not build or test the project.
    *   **AUDIT VERDICT**: Every audit ends with one of: **PASS** (issue next prompt), **PASS WITH NOTES** (every note is fixed before the commit through same-session fix prompts, then the next prompt is issued; nothing is carried forward as known debt, Adam 2026-11-06), **FAIL** (issue a corrective prompt for the same step; do not advance the gate). Adam commits only when a whole task is finished (he uses `git restore .` to roll back in between); at that point give him a `git commit -m "…"` line in a code box (commit command only). **Command format (Adam, 2026-11-04):** any command he must paste goes in its own unindented code block, never nested inside a list item, with no leading spaces. **Push policy (Adam, 2026-11-05):** push to `origin main` at the end of every gate: after the Orchestrator's verdict is PASS or PASS WITH NOTES (no unresolved defect in the commit), the gate commit and the docs commit are both made, and `git status --short` is clean. Never push mid-gate, after a FAIL, or with a verdict pending. Never force-push, never push to `upstream` (disabled). The Orchestrator ends every "Your steps" list with the push command when a push point is reached, and says "no push yet, because ..." when it is not. Between steps nothing is committed, so every prompt's "Last verified" line lists all files the task has modified so far.
*   **Coding Agent (Qwen3.8-27B via llama.cpp, inside CLion; the only coding agent; harness recorded in HANDOFF.md §2b). Gemma is permanently banned by Adam's decision; never use, suggest, or prompt for it.**: Executes code changes from AGENTS.md rules.
    *   **PROTOCOL**: The Agent executes exactly ONE logical step per turn and **MUST** end every response with "STOPPING AND WAITING FOR VERIFICATION". It has no memory between sessions.

## 2. The Gate System (Workflow Protocol)
Every non-trivial task executes under this protocol. No exceptions.
1.  **Step 0 (Read-Only Audit)**: Grep, read, and report. Zero code changes. Output a categorized hit report with file:line references. STOP AND WAIT.
2.  **Step 1..N (One Step at a Time)**: Execute exactly one logical step. Output verbatim quoted code, an edited-file manifest, and grep proofs. End with "STOPPING AND WAITING FOR VERIFICATION" and CEASE ALL GENERATION.
3.  **Self-Contained Prompts**: The agent is cleared between sessions. Every prompt must carry `CONTEXT (BINDING)`, `STRICT RAILS`, `STEPS`, and `EVIDENCE` requirements.
    **Mandatory opening block.** Every fresh prompt (new task, or any prompt Adam may send to a freshly wiped agent) MUST begin with this block, verbatim except the bracketed line:
    ```
    FIRST: read AGENTS.md in the project root before doing anything else. Its rules are binding for this entire task and override anything below if they conflict. If you cannot read it, say so and STOP.
    Last verified: [one line — e.g. "Step 1 PASS, tree clean at <short HEAD>" or "New task; tree clean at <short HEAD>"]
    ```
    **Adam starts a fresh agent chat for every new step** (keeps the local model's context small). So every step prompt the Orchestrator issues is a full, self-contained fresh prompt; never a bare "Proceed". **Exception, fix loops (Adam, 2026-11-06):** a fix for an audit note goes to the same agent session as a short prompt headed `CONTINUATION (same session): <gate> fix <n>. AGENTS.md still binds.` plus a "Last verified" line listing the uncommitted files, the exact change, the raw evidence wanted and the stopping phrase (AGENTS.md rule 17). If Adam says the agent is low on context, the fix is rolled into the next full prompt instead. **Debt definition:** defects, missing includes, wrong types, missing purpose comments, untested paths of code that exists, and stale docs are fixed now; a remark that depends on code not yet written is attached to the gate that writes it (in docs/roadmap.md), never kept as a loose note. Commit rhythm: gate step, audit, fix loops until no note is open, then one gate commit (fixes included), the docs commit, push. The "Last verified" line lists every file the task has modified so far (uncommitted) so the agent builds on them and stops on anything else.
4.  **Final step of every task**: configure + build + full test run with exact counts, then a documentation sweep (README, changelog, ARCHITECTURE.md) unless Adam defers it.
5.  **Agent context wipes are frequent.** A correction given only inside a prompt is lost on the next wipe. Any correction that should outlive the current step goes into AGENTS.md (Adam commits it); the prompt may repeat it, but AGENTS.md is the durable copy. Every continuation prompt must state the last verified state in one line.
6.  **Keep gates small.** Local models degrade under large evidence loads (repetition loops, silently dropped hits). Aim for roughly 10 hits needing context per gate; use counts-only gates for high-volume sweeps; at most 10 commands per tool call.
7.  **Milestone review (Adam, 2026-11-06).** When the last gate of a milestone (M0, M1, ...) and its documentation sweep are committed and pushed, the Orchestrator does not write the next milestone's first prompt. It opens a discussion with Adam instead: what was accomplished (against what the roadmap promised, with the test count), what was learned (process: agent behaviour, prompt mistakes, rules that worked or did not; technical: library quirks, design choices that held or did not), and what the next milestone is (purpose, gates, risks, open decisions, and whether the plan should change). The next milestone starts only when Adam agrees. The conclusions go to `docs/milestones/M<n>-review.md`; lessons that should bind future work also go into AGENTS.md or these docs, and HANDOFF.md records the review as done. Gates inside a milestone are not reviewed this way.
8.  **Prompt-authoring practices (Adam, 2026-11-06).** So that gates do not fail on form:
    - Every prompt that creates or changes `.cpp` or `.h` files has one step just before the build: `Run: clang-format -i <exact list of the .cpp and .h files this step creates or changes>; echo "exit=$?"` (AGENTS.md rule 19 is the durable copy; the explicit step is what the local model reliably follows).
    - Never dictate whitespace (indent widths, line breaks); give the code and its meaning and let clang-format lay it out. Any code quoted in a prompt is already in clang-format layout (`.clang-format`: LLVM base, indent 4, 100 columns, access labels flush, `template <...>` on its own line, short functions inline).
    - Every prompt that creates a source or script file states its first lines exactly: the SPDX line, the copyright line and the purpose comment (AGENTS.md rules 5 and 16), because clang-format cannot add them and `SourceCheck.Repo` fails without them.
    - Long outputs are marked FILE and go to `evidence/<gate>/` (AGENTS.md rule 18); short ones are pasted raw.
    - Prompts and AGENTS.md never say what the Orchestrator itself checks or reads.
    - Check every tool option dictated in a prompt against the versions on Adam's machine (clang-format 18.1.3: the key is `AlwaysBreakTemplateDeclarations`, not `BreakTemplateDeclarations`).

## 3. Architectural Rails (Decided; see ARCHITECTURE.md)
1.  **Greenfield.** Nothing is inherited from CrimsonEditor. Licence GPL-3.0-or-later; every source file carries the SPDX line and the copyright line (AGENTS.md rule 5).
2.  **C++17 with the plain GTK 4 C API, GtkSourceView 5.** No gtkmm, no libadwaita, no bindings, no vendor SDKs, nothing that merely wraps another API. A new dependency needs Adam's decision.
3.  **No C++ exceptions in project code; RAII only.** `GError` at the GTK boundary.
4.  **Layers.** `src/core` and `src/agent` never include GTK, Pango, GtkSourceView or VTE headers; `src/ui` holds all UI.
5.  **UTF-8 internally; explicit encodings and line endings at file boundaries; XDG paths** for config, data, cache and state.
6.  **Responsive UI.** The GTK main loop is never blocked; async I/O on the main loop, threads only for heavy computation.
7.  **Agent safety.** Permission presets (Restricted is the default; Balanced; Unrestricted behind a confirmation and a red badge; Custom). Hard guards in every preset: the agent cannot change its own preset and cannot read API keys. Keys live in the keyring; JSON holds references only.
8.  **Tests with every gate.** Core and agent logic are tested without a display; UI stays thin.
9.  **Identity stability.** Never rename config keys, JSON fields, file formats or command names when only display text changes.

## 4. Project Summary & Component Map
**Vermilion Editor**: native Linux text and code editor, written from scratch in C++17 on GTK 4 and GtkSourceView 5, with a built-in AI agent (OpenAI-compatible, Anthropic and Gemini providers, MCP), build and Output/Problems panes, a DAP debugger, a VTE terminal, a right-hand directory sidebar, and a model monitor ported from Adam's `ctxmon`. IDE: CLion (CMake). Local working tree: `~/Projects/vermilion-editor`. Remote: `origin` = https://github.com/adamjeremy-sk/Vermilion-Editor.

Component map: `src/core` (UI-free logic), `src/agent` (providers, loop, tools, MCP), `src/ui` (GTK application), `tests/`. Details and the planned directory names are in ARCHITECTURE.md section 4; the milestone plan is docs/roadmap.md. Earlier history (the abandoned Crimson port) is in `docs/archive/`.

## 5. Known Traps & Common Agent Drifts
*Process drifts (seen with the local agent):*
*   **Drift**: Proceeding to the next step without waiting for Adam's authorization.
*   **Drift**: Putting "..." inside a raw-output block, retyping a diff by hand, or summarizing instead of pasting raw output (seen repeatedly). Raw output is compared against the real command output.
*   **Drift**: Silently omitting hits it judges irrelevant instead of classifying them. Always reconcile table rows against raw output lines.
*   **Drift**: Claiming "zero references" without showing the grep and its raw output.
*   **Drift**: Drive-by edits (include cleanup, reformatting) outside the step's scope.
*   **Drift**: Inventing APIs, headers or function names. Adam's compile is the only proof; a gate that adds code says exactly what to build and run.
*Technical traps for this stack:*
*   **Trap (GTK 3 habits)**: The model tends to write GTK 3 code. In GTK 4 there is no `gtk_container_add`, no `gtk_widget_show_all`, no `pack_start`; use `gtk_box_append`, `gtk_window_set_child`, `GtkEventController`s, `GtkListView`/`GtkColumnView` (not `GtkTreeView`, deprecated), `GtkFileDialog` (not `GtkFileChooserDialog`). GtkSourceView is version 5 (`GtkSourceView`, `GtkSourceBuffer`, `GtkSourceMark`), libsoup is version 3 (`SoupSession`, `soup_session_send_async`); both differ from the older versions in the model's memory.
*   **Trap (ownership)**: GObject reference rules (transfer none/full, floating references, `g_object_ref_sink`), and signals still connected to a destroyed object. Use the `gtkx` helpers; never leave a bare `g_signal_connect` capturing a pointer that can die.
*   **Trap (tests)**: `EXPECT_EQ(const char*, "literal")` compares pointers and always fails; compare `std::string` values or use `EXPECT_STREQ`. A gate prompt must name the comparison, not just say "equals" (my M0.2 prompt did not).
*   **Trap (exceptions)**: An exception escaping through a GTK callback is undefined behaviour. No exceptions in project code.
*   **Trap (main loop)**: Blocking calls (synchronous HTTP, `waitpid`, large file reads) on the main thread; widgets touched from a worker thread.
*   **Trap (streaming)**: Server-sent events arrive in arbitrary chunks; a parser that assumes one event per read breaks. Tool-call arguments also arrive in fragments.
*   **Trap (secrets)**: API keys must never reach logs, JSON files, error messages or the request log.
*   **Trap (encodings)**: Never guess an encoding silently; state it. Detect line endings and preserve them.
*   **Trap (paths)**: Linux paths are case-sensitive and may not be valid UTF-8; use the path APIs.

## 6. Hand-off Protocol (Orchestrator continuity)
Orchestrator chats end (context limits, new day, model switch). HANDOFF.md is the single source of truth for "where are we". It lives in the claude.ai Project (so every new chat in the Project sees it) and in the repo for history.

**6.1 Session start (new Orchestrator instance)**
0. **First action once the Project docs are read: get the repo folder.** (This file and HANDOFF.md are read from the claude.ai Project with the Projects tool, `claude/<name>`; a fresh chat's working directory is empty, so never look for them on disk.) Call `get_device_info`; if `connectedFolders` lacks `/home/adam/Projects/vermilion-editor`, call `device_request_folder_access` for that whole path (the grant covers the subtree). Ask once; if denied or unanswered, do not repeat, tell Adam and fall back to gated Qwen prompts or ask him to attach the folder. Use `device_bash` afterwards (mounted at `$HOME/mnt/vermilion-editor`, read-only git with `--no-optional-locks`).
1. Read AI_ONBOARDING.md, ARCHITECTURE.md, HANDOFF.md.
2. If the repo exists, compare HEAD with the commit recorded in HANDOFF.md. If they differ, read `git log` between them and note what changed.
3. Reply to Adam with a **State Readback** (max 8 lines): version/HEAD, active task, gate position, what is awaited next, open decisions, and any drift found. Do nothing else until Adam confirms or corrects it.
4. If the active task is mid-gate, resume from the "Next action" field — typically re-issuing or continuing from the recorded last prompt.

**6.2 Updating HANDOFF.md (Orchestrator)**
Update it (and write it back to the Project) at each of these moments, without being asked:
- a task completes its final gate;
- Adam makes an architectural decision;
- Adam says "handoff", "wrap up", or that he's ending the session;
- the conversation is getting long (risk of context loss).
Rules: overwrite §1 Current State entirely; append one dated line per decision to §2; edit §3 Backlog; prepend a §4 Session Log entry and keep only the 10 newest. Record the last issued prompt verbatim in §1 when a task is mid-gate — a new instance cannot recover it otherwise.

**Mirror rule (Adam, 2026-11-04):** the repo is authoritative. Whenever the Orchestrator changes HANDOFF.md, AGENTS.md, AI_ONBOARDING.md, ARCHITECTURE.md or docs/roadmap.md, it also overwrites the matching file in the claude.ai Project (`claude/<name>`) in the same turn, so no stale copy remains. Stale Project copies mislead new instances. Delete superseded drafts from the Project.

**6.3 Adam's side**
- To start a new chat: open it inside the Vermilion Editor Project and send "Resume from HANDOFF.md."
- After a handoff update, commit the refreshed HANDOFF.md (and any changed docs) to the repo.
