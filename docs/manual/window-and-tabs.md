# The window and tabs

## The window
From top to bottom: the menu bar (File, Edit, View, Help), the document tabs with the editor, and the status bar. There is
no toolbar: every command is in a menu, and the menu shows its shortcut. Hold Alt to see the underlined access letters in
the menu bar.

## Documents and tabs
- Vermilion starts with one new document, "Untitled 1". File → New File (Ctrl+N) opens another. A new untitled document
  takes the lowest number not in use.
- Every tab has the same width. A long name is cut short with "…"; hover over the tab to see it in full.
- A `*` before the name means the document has changes.
- Close a tab with its × button, a middle click on the tab, or File → Close Tab (Ctrl+W).
- When the tab you are working in closes, the tab to its left becomes active (the one to its right if it was the first).
  Closing any other tab leaves the active tab as it is.
- Drag a tab to move it. When there are more tabs than fit, use the arrows at the ends of the tab strip or the mouse wheel
  over the tabs.
- Vermilion never closes a tab on its own.

## Closing with changes
Saving files arrives in a later version. Until then:
- Closing a tab with changes asks whether to close it without saving. Close Without Saving discards the changes; Cancel
  (also Escape or Enter) keeps the tab.
- Quitting (File → Quit, Ctrl+Q, or the window's close button) with changed documents asks once, for all of them.

When the last tab closes, the window stays open and empty, and the commands that need a document are greyed out until you
open a new one.

## The status bar
On the left, messages. On the right, for the active document: the language (Plain Text for now), the tab width (4), the
line and column of the cursor (a tab counts as the columns it covers), and INS or OVR (the Insert key switches between
inserting and overwriting).
