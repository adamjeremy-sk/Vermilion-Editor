# Find, Replace and Go to Line
The Search menu holds Find…, Find Next, Find Previous, Replace… and Go to Line…. Find, Replace and Go to Line are small
windows that stay open while you work in the document; the Search menu works on the active document.

## Find
Find… (Ctrl+F) opens the Find window. Type the text and press Enter, or click Find Next, to move to the next match; Find
Previous goes the other way. When the search runs past the end of the document it wraps around, and the window says so.
Close, or Escape, closes the window.

- Find Next (F3) and Find Previous (Shift+F3) repeat the search from the menu or the keyboard, also when the window is
  closed.
- Match case: only text with the same capital letters matches.
- Match whole word only: the match must be a whole word, not part of a longer word.
- Regular expression: the text is a pattern, in the syntax of GLib (close to Perl). With it off, every character is
  literal, a backslash included.
- All matches in the document are highlighted. The window shows which match you are on and how many there are, for example
  "3 of 12 matches". A pattern that is not valid is explained in the window, and nothing is searched.

## Replace
Replace… (Ctrl+H) opens the Replace window, with the same options as Find and a second field, Replace with.

- Replace changes the current match and moves to the next one.
- Replace All changes every match in the scope at once. Choose the scope above the buttons: Whole document, or Selection
  only (the text that was selected when you opened the window or chose the scope).
- With Regular expression on, the replacement may use `\1` to `\9` or `\g<name>` for a group of the pattern, `\0` for the
  whole match, `\n` and `\t`, and the case switches `\U`, `\L`, `\u`, `\l` and `\E`. A replacement that names a group the
  pattern does not have is refused with a message, instead of deleting text. With it off, the replacement is literal.
- Replace All is one step of Undo, however many matches it changes. Edit → Undo takes the whole run back.

### Large documents
On a big document, Replace All works in short slices so that the program stays alive: the window shows "Replacing: 12,000
of about 289,000", the document is locked against editing, and the Replace All button becomes Cancel. Cancel stops the run
and keeps what was replaced so far; that part is one step of Undo. While the run goes, Undo and Redo do nothing.

Two things to know, measured on a text of 50 MB with 289,000 matches:
- Replace All changes about 4,500 matches a second, so that run took about a minute.
- Undoing a very large run takes longer than the run did, and the window does not answer while it works. Undoing 19,000
  matches took about 10 seconds. Let it finish.

Every open copy of such a file also takes memory of its own, roughly 18 times the file's size, so close copies you no
longer need.

## Go to Line
Go to Line… (Ctrl+G) asks for a line number, or `line:column`, for example `120` or `120:8`. The window names the range of
lines that exist. A number past the last line goes to the last line and says so. Press Enter or click Go.

## The current line
The line with the cursor is shaded, so it is easy to find. The shade follows the colour mode. You can turn it off with
**Highlight the current line** in Tools → Settings…, on the Appearance tab (see [Settings](settings.md)).

## Limits
- A document of one enormous line with many highlighted matches may make the window slow or freeze, because the matches
  are highlighted in the display. Search for something rarer in such a file.
