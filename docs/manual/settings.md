# Settings

## Opening the Settings window
Tools → Settings… or Ctrl+Comma (Ctrl+,) opens the Settings window. It has three tabs, Appearance, Editor and Files,
and three buttons:

- **OK** applies the changes and closes the window.
- **Apply** applies the changes and keeps the window open. It is greyed out until something has changed.
- **Cancel** closes the window and throws the changes away. Esc and the window's close button do the same.

Nothing changes in the program until you press Apply or OK. After that the change shows within a moment, with no
restart. The window reads the settings file again every time it opens.

## Appearance
- **Colour mode** is Light, Dark or Follow system. Follow system, the default, uses the desktop's own light or dark
  setting and follows it when it changes; on a desktop without one, a GTK theme whose name ends in "-dark" counts as
  dark. Light and Dark can always be chosen by hand. The colours of the editor text follow the mode.
- **Highlight the current line** shades the line the cursor is on. It is on by default.
- **Use the system monospace font** is off by default: the editor uses Fantasque Sans Mono 12, the font that comes with
  Vermilion. Turn the box on to use the desktop's monospace font instead, or choose another font with the font button.
  Only fixed-width fonts are offered.

## Editor
- **Tab width** is 4 or 8. The Tab key inserts that many spaces. The status bar shows the width as "Tab Width".

## Files
- **Ask before reloading a large file (over 10 MB) from disk** is the same setting as "Do not ask again" in the
  question that File → Reload from Disk asks for a large file; see [Files changed on disk](files-changed-on-disk.md).
- **Ask before opening a file with a line longer than … characters** sets when Vermilion asks "Open Anyway" or "Cancel"
  before it opens a file; see [Opening and saving files](open-and-save.md). The number is from 10,000 to 5,000,000. The
  default is 350,000.

## The settings file
The settings are kept in `~/.config/vermilion/settings.ini` (or under `$XDG_CONFIG_HOME`). Vermilion writes only the
keys that differ from the defaults, and keeps the keys and groups it does not know.

| Group | Key | Values | Default |
|---|---|---|---|
| `[appearance]` | `colour_mode` | `light`, `dark`, `system` | `system` |
| `[appearance]` | `highlight_current_line` | `true`, `false` | `true` |
| `[editor]` | `font` | a font name such as `DejaVu Sans Mono 11`; empty means the system monospace font | `Fantasque Sans Mono 12` |
| `[editor]` | `tab_width` | `4`, `8` | `4` |
| `[files]` | `confirm_large_reload` | `true`, `false` | `true` |
| `[files]` | `long_line_limit` | 10000 to 5000000 | 350000 |

A value that is not allowed is replaced by the default (a number outside its range is moved to the nearest end). A
settings file that cannot be read is ignored for that run and replaced by a new one the next time you apply a change
(the old one is kept as `settings.ini.bad`). If the file cannot be written, the status bar says "Could not save the
settings" and the program goes on with the new values until it is closed.
