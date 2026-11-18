# Files changed on disk

Another program can change a file that is open in Vermilion: a formatter, a build, `git checkout`, a sync tool or
another editor. Vermilion looks at the file of the tab you are working in and tells you, or acts, as described here.

## What Vermilion does
- **A document without unsaved changes** whose file is 10 MB or smaller is reloaded by itself. The status bar says
  "Reloaded “name” (changed on disk)". The cursor and the scroll position stay, and the undo history is cleared,
  because the new text is the new starting point.
- **A document with unsaved changes** gets a bar above the text: "“name” changed on disk." **Reload** replaces your
  text with the file's. **Keep mine** keeps your text; the bar goes away and does not come back until the file changes
  again.
- **A file over 10 MB** is never reloaded behind your back, even when the document has no changes. The bar says that
  it is large and was not reloaded; **Reload** reloads it and **Ignore** keeps the text as it is.
- When the reloaded file looks binary or has a very long line, Vermilion asks "Reload Anyway" or "Cancel" first, as
  when opening a file. If the encoding found is not the one the document had, the status bar says so, unless you chose
  the encoding yourself.

## When nothing happens
Saving from Vermilion, `touch`, and a program that writes the same text again do not show anything: Vermilion compares
the time and the size of the file, then its contents, and acts only when the text really differs.

## A file that was deleted or moved
- The tab keeps its text and gets the `*`, because the text now exists only here. The status bar says "“name” was
  deleted or moved on disk".
- Closing the tab or quitting asks "Save “name” before closing?", and a recovery copy is kept (see [Crash
  recovery](crash-recovery.md)).
- If the file comes back, a document without changes is reloaded by itself; otherwise the bar says "“name” is back on
  disk after it was deleted or moved." **Reload** takes the file's text; **Keep mine** keeps yours, marked as changed
  so that Save writes it.
- Save works as usual: it writes the file again.

## Which tabs are watched
Only the tab you are working in is watched. A tab in the background is checked when you switch to it. Every tab is
checked when you quit, and a tab is checked when you press its close button. When you quit with a document that also
changed on disk, its question says "This file also changed on disk." Network and sync folders send no change messages,
so for them the check happens when you switch to the tab.

## Saving over a changed file
If a file changed on disk since you opened or last saved it and you save, Vermilion asks "Save Anyway" or "Cancel";
see [Opening and saving files](open-and-save.md). After **Keep mine** or **Ignore** this question does not come back
for that version of the file.

## File → Reload from Disk (Ctrl+R)
Reloads the active document from its file by hand, also when the file did not change. The status bar says "Reloaded
“name” from disk".
- A document with unsaved changes asks "Discard your changes and reload from disk?" first.
- A document without changes whose file is over 10 MB asks "Reload “name” from disk?" with a box "Do not ask again".
  With the box ticked, Reload goes ahead and later large files reload without the question.
- The command is greyed out for an untitled document, while the document loads or saves, and during a Replace All.
- Cancel, Escape and the window's close button leave everything as it was.

## The setting
The "Do not ask again" box is stored in the settings file, `~/.config/vermilion/settings.ini` (or under
`$XDG_CONFIG_HOME`), as `confirm-large-reload` in the group `[files]`. Set it to `true` to be asked again. The
Settings window of a later version will offer it. A settings file that cannot be read is ignored for that run:
Vermilion uses the default (ask) and says so in the status bar.
