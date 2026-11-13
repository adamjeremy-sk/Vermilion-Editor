# Crash recovery

Vermilion keeps a recovery copy of every document with unsaved changes, so a crash, a power cut or a killed program does
not cost you your text.

## What is copied
- Every document with unsaved changes, untitled ones included, is copied about 5 seconds after you stop typing, and at the
  latest 30 seconds after the first unsaved change since the last copy. Copies are written in the background.
- A document that is still loading, and a read-only document, gets no copy.
- A copy is deleted when you save the document or close it without saving, and when you quit normally.
- A document of more than 100,000,000 characters gets no copy, and the status bar says "Too large for recovery copies".
- If a copy cannot be written (the disk is full, the folder cannot be used), the status bar says "Could not write a recovery
  copy." once, and "Recovery copies are working again." when they work again.

## Where the copies are kept
- In your state folder, under `~/.local/state/vermilion/recovery/light/` (or under `$XDG_STATE_HOME` when that is set). Each
  copy is a file with a random name, so the name does not reveal what you were editing, and only you can read it.

## The question at the next start
- When the last run did not end normally, Vermilion asks about each copy it finds, one at a time, once the window is
  shown. The question names the document, says "(2 of 3)" when more than one is waiting, shows when the copy was made and,
  for a document that belongs to a file, which file and whether the file has changed since.
- **Recover** opens the text in a new tab, marked as changed.
- **Discard** asks "Delete this copy?" first. Delete removes the copy for good; Cancel goes back to the first question.
- **Decide Later** (also Escape) keeps the copy and asks again at the next start.
- **Recover All** appears when two or more copies are waiting. It recovers every copy that can be read, in order, without
  more questions.
- A copy that cannot be read is called damaged. For it you can only Discard or Decide Later.
- When the last question is answered and some copies were kept, the status bar says how many.

## A recovered document
- It opens with its own name when it had one. If that file is already open in another tab, the text opens as an untitled
  document instead and the status bar says so.
- The blank "Untitled 1" that Vermilion starts with is closed when the first copy is recovered, as long as you have not
  typed in it.
- A recovered document has no undo history. Its recovery copy is kept, and renewed as you type, until you save it or close
  it without saving.
- The first Save of a recovered file document checks the file. If the file changed on disk after the copy's text was based
  on it, Vermilion asks "Save Anyway" or "Cancel", as described in [Opening and saving files](open-and-save.md).
