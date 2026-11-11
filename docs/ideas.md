# ideas.md: Potential features (unscheduled)
**Purpose**: a parking place for ideas as they come. Nothing here is planned, scheduled or promised.
**Rule (Adam, 2026-11-10)**: the roadmap does not grow until version 1.0.0-stable is reached; only then are other features considered. Only Adam moves an idea from this list into `docs/roadmap.md`.
Features that were decided against earlier are listed under "Dropped" in ARCHITECTURE.md section 2; they come back here only if Adam brings one back.

| Idea | Added | Notes |
|---|---|---|
| Vi / Vim key bindings (optional, switched on in settings) | 2026-11-10 | GtkSourceView 5 appears to provide a Vim input method (`GtkSourceVimIMContext`); to be confirmed against version 5.12 before any promise. |
| Emacs key bindings (optional) | 2026-11-10 | No ready-made support in GtkSourceView or GTK 4; it would be our own key handling, so a larger job than Vim keys. |
| Tab limit (optional): when more than N tabs are open (for example 20), the least recently used one closes, never one with unsaved changes, with a status bar message | 2026-11-10 | Planned for M2.6, dropped by Adam before M2.1: Vermilion does not close tabs on the user. It would need a recent-use counter in `DocumentList`. |
