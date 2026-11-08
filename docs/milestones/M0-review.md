# M0 milestone review: Reset and foundation
**Held**: 2026-11-07, Adam and the Orchestrator. **Last M0 commit**: `e6a7ed0` (docs sweep), code at `11d817c`. **Tests**: 67, all passing, none skipped.

## 1. Accomplished, against the roadmap
Everything the roadmap promised for M0 was delivered:
- A CMake skeleton with the `core`, `agent` and `ui` layers, GoogleTest, `.clang-format`, and an empty GTK 4 window with the application ID. Adam confirmed that the window opens.
- `gtkx`: `ObjectPtr` for GObject ownership, signal connections by lambda with RAII disconnect, and string helpers, all tested without a display.
- `Error`, `Result<T>` and `Status`, with death tests for misuse.
- `redact_secrets`, and a log layer over `g_log` that redacts every message.
- Repository checks that run as ctests: `SourceCheck.Repo` (SPDX and copyright header, purpose comment, file size) and `Format.Check` (clang-format 18.1.3, skipped when clang-format is missing).
- The history was rewritten to start at the reset commit.

Changes to the plan:
- Three gates were added (M0.5d1 to d3) to clear debt and do a one-time format pass.
- M0.5b and M0.5c were each split in two.
- That made 12 gates, and every one ended PASS. Two needed a FAIL and a fix loop on the way: M0.5d1 (raw output was summarised, and lines dictated by the prompt were too long) and M0.5d3 fix 1 (the Orchestrator gave a clang-format key that version 18 does not have).

## 2. Learned
### Process
1. **Evidence files work.** Qwen replaced raw output with summaries twice (M0.5d1, M0.5d2). Since AGENTS.md rule 18 (long output written by the command itself to `evidence/<gate>/`), M0.5c1 and M0.5c2 passed first time with no notes. This rule did the most for audit quality.
2. **Most failures came from the prompts, not from Qwen:** `EXPECT_EQ` on `const char*`, C++ lines over 100 columns dictated by the prompt, and a clang-format option that version 18 does not have. Qwen reported each one instead of working around it. The answer is the checklist in AI_ONBOARDING.md section 2 item 8, which stays binding.
3. **Fixing every note before the commit (rule 17) is cheap.** Same-session continuation prompts cost little, and M0 closed with no open debt.
4. **Gate size.** Gates with about 10 hits that need context were stable, and every split gate went smoothly afterwards. The rule stays.
5. **Formatting.** Matching `.clang-format` to how Qwen writes (rule 19, plus the explicit format step in every prompt) avoids format failures. Fighting its style would not.
6. **Minor:** Qwen writes "(empty)" for empty output. It is harmless now that long output goes to files.

### Technical
1. An INTERFACE target with compile options, linked PUBLIC, passes `-fno-exceptions` on to the tests (M0.2, fixed in M0.3). Link such targets PRIVATE (AGENTS.md rule 20).
2. GLib 2.80's `g_object_add_weak_pointer` does not set the pointer when it is added, so the tests preset it.
3. `std::function` and lambdas compile fine under `-fno-exceptions`.
4. Under GoogleTest, compare C strings with `EXPECT_STREQ` or as `std::string` (AGENTS.md rule 20).

## 3. Next milestone, M1, and decisions taken
- **A. Plan accepted (Adam).** M1 is core work only; the GUI comes later. The revised gates are M1.0 (scaffold, fixtures, oracle script) and M1.1 to M1.6 (docs/roadmap.md).
- **A2. ctxmon is now developed here (Adam).** Vermilion's ctxmon is the source of truth, and development moves into this repository. The Python original in `reference/ctxmon/` is frozen and serves only as the test oracle. Layout: `src/ctxmon/core` (no GTK), `src/ctxmon/ui`, `tests/ctxmon`. Core and UI never mix.
- **B. Regular expressions (Adam).** GLib `GRegex` (PCRE2), which is part of GLib, so no new package is needed. It will be used for search later. `std::regex` is not used.
- **C. When the GUI is built (Adam).** ctxmon is finished during M2: settings (M2.7), I/O on the main loop (M2.8), cards and graphs (M2.9), and the standalone `vermilion-ctxmon` (M2.10). The CTXMon tab follows in M3.6, and ctxmon moves from v0.4 to v0.1.
- **D. Doc sweeps (Adam).** The Orchestrator does every documentation sweep. README.md is brought up to date when Adam says "Handoff to tomorrow's session", not after every change (AI_ONBOARDING.md section 6.2).
- **E. All references to the earlier code base are removed (Adam).** This covers the files and the Project docs only. Git history is kept as it is.

Risks for M1:
- Lookbehind in the generation-speed pattern (answered by `GRegex`).
- Fixtures must cover old and new llama.cpp wording, truncation and partial lines.
- Oracle drift (answered by freezing the Python).
- File-size limits on the test files, which hold data tables (500 and 800 lines).
