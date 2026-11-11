# Opening and saving files

## Opening
- File → Open… (Ctrl+O) shows the file dialog, which starts in your home folder. The file opens in a new tab. A file that
  is already open just becomes the active tab.
- Before a file loads, Vermilion reads it once from start to end ("Checking “name”… 40%" in the status bar) to find its
  encoding, its line endings and whether it looks like a text file. Then it loads ("Loading “name”… 40%"). The tab cannot be
  edited until the load ends, and the syntax colouring is switched on after it, which keeps big files fast.
- A file that contains NUL characters looks like a binary file. A file with a line longer than 350,000 characters can make
  any text editor slow or stop responding. For both, Vermilion asks "Open Anyway" or "Cancel" first. A binary file opened
  anyway is read-only.
- A file with bytes that cannot be shown as text in the encoding it was read in also opens read-only, and the status bar
  says so. A read-only document cannot be saved: File → Save is greyed out.
- When the file is not plain UTF-8 the status bar says how it was read, for example "Opened “name” as Windows-1252". See
  [Encodings and line endings](encodings-and-line-endings.md).
- View → Word Wrap switches word wrap on or off for the active document. Every document opens without it.

## Saving
- File → Save (Ctrl+S) writes the document back to its file. For an untitled document it asks for a name, like Save As.
- File → Save As… (Ctrl+Shift+S) asks for a name (an untitled document is offered in your home folder) and saves there.
  The tab then shows the new name and the syntax colouring follows the new file type. If the name belongs to a file that is
  open in another tab, Vermilion refuses and says so.
- The status bar shows "Saving “name”… 40%" and then "Saved “name”." The document cannot be edited while it loads or saves.
- Vermilion writes what you see: a file that had no line break at its end does not get one, and a byte-order mark is kept.
  The text goes into a new file beside the original, which is then swapped in, so a failed save leaves the old file as it
  was.
- If the file changed on disk since you opened it or last saved it, Vermilion asks "Save Anyway" or "Cancel". Save Anyway
  replaces the version on disk with yours.
- If saving fails (the disk is full, the folder is read-only), the document stays unsaved and a message gives the reason.

## Closing and quitting
- Closing a tab with changes asks Save, Close Without Saving or Cancel. Save saves first and then closes (for an untitled
  document it asks for a name; cancelling that cancels the close). Cancel, Escape or Enter keeps the tab.
- Quitting (File → Quit, Ctrl+Q, or the window's close button) asks about each changed document in turn and shows its tab.
  Save and Close Without Saving go on to the next one; Cancel stops the quit.
