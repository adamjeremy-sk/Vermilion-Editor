# Encodings and line endings

## Encoding
- Before a file loads, Vermilion picks its encoding: UTF-8 if every byte is valid UTF-8; UTF-16 LE or BE if the file starts
  with that byte-order mark (BOM); otherwise Windows-1252, or Latin-1 when the file contains one of the five bytes that
  only Latin-1 defines.
- The Encoding panel in the status bar always shows it: UTF-8, UTF-8 with BOM, UTF-16 LE, UTF-16 BE, Windows-1252 or
  Latin-1. Click the panel for its menu.
- **Reopen as** reads the file again from disk in the encoding you choose (UTF-8, UTF-16 LE, UTF-16 BE, Windows-1252,
  Latin-1). Use it when Vermilion guessed wrong. A document with unsaved changes asks first: Save and Reopen (saves in the
  current encoding, then reloads), Discard Changes and Reopen, or Cancel. A file whose bytes are not valid in the chosen
  encoding opens read-only.
- **Save as** makes the next save write the encoding you choose: UTF-8, UTF-8 with BOM, UTF-16 LE, UTF-16 BE, Windows-1252
  or Latin-1. The document counts as changed, so Save writes it. A character the encoding cannot hold makes the save report
  an error.
- A byte-order mark stays at the start of the text as an invisible character and is written back. UTF-16 always has one.
  Choosing Save as → UTF-8 with BOM adds the mark, and Save as → UTF-8 removes it.

## Line endings
- The Line Ending panel shows LF, CRLF or CR. New documents use LF.
- A file that mixes several kinds of line ending shows Mixed, and the status bar names the ending the next save will write
  on every line: the one most lines have.
- Click the panel and choose Convert to → LF, CRLF or CR. The next save writes that ending on every line.
