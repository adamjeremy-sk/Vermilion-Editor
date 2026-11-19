# Command-line options

## Opening files from a terminal
`vermilion file1 file2 ...` starts the editor and opens each file in its own tab, in the order given. A relative name
is taken from the folder the command was typed in. A file that is already open just becomes the active tab. `vermilion`
without a name opens a blank window.

If the editor is already running, the files open as new tabs in that window and the window comes forward. A blank
document that has not been touched is replaced by the first file that opens; text you have typed is never replaced.

## Files that do not exist
For each name that does not exist, the terminal asks `'name' does not exist. Create it? (Y/n)`. Y, yes or just Enter
creates an empty file and opens it; n or no skips the name. Anything else asks again. The editor stays usable while a
question waits in the terminal, and Ctrl+C in that terminal ends only that command, not the running editor.

When there is no terminal to ask (the command was started from a launcher, or its input is redirected), nothing is
created and the status bar says "name does not exist".

A name whose folder does not exist cannot be created: the terminal and the status bar say "Could not create name" and
give the reason.

## What is refused
A folder ("name is a directory, not a file"), a device or another entry that is not a regular file, and a location that
is not on this computer are not opened. The reason is printed in the terminal and, once the other files are done, shown
in the status bar.

## Options
- `--help` or `-h` prints a short usage text and exits without opening a window. To open a file that is really named
  like an option, write it with a folder, for example `./--help`.

Every later option is described on this page.
