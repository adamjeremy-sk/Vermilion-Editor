# Using the manual
The manual opens from Help → Index, or with F1. It has one short page for each topic. It opens on the list of topics,
with the Contents tab of the side pane in front.

## The window
The manual has its own window, and there is only one: choosing Help → Index again brings the open window forward. The
editor stays usable while the manual is open. Close the manual with its close button; it also closes when the editor
closes.

At the top are the buttons **Hide**, **Back** and **Forward**. Below them the side pane is on the left and the page on
the right; drag the divider between them to change their widths. A status line at the bottom shows the search counts and
messages. If you open the manual just after the editor starts, the page may say "Loading..." for a moment.

The page uses the system monospace font and the editor's colours, light or dark. Text and tables follow the width of
the window; a table that cannot get any narrower scrolls sideways.

## Moving around
- Click a link to open another page. Links are not underlined; the pointer becomes a hand over them.
- **Back**, or Alt+Left, returns to the page you came from. It is greyed out on the first page.
- **Forward**, or Alt+Right, goes to the page that Back left. It is greyed out when there is none.
- Scroll with the mouse wheel or the scroll bar. A page opened by a link starts at the top.
- Ctrl+= makes the text larger, Ctrl+- smaller, and Ctrl+0 returns it to its normal size. The +, - and 0 keys of the
  number pad work too, with Ctrl.

## Contents
The first row of the Contents tab is the list of topics; then come the topics in the same order, each with its headings
below it. The topic of the page you are reading is open and its row is marked. Click a row to open its page; click a
heading to go straight to it. A topic you open or close by hand stays as you left it.

## Search
- Ctrl+F shows the side pane, switches to the Search tab and puts the cursor in the search box. Type and press Enter.
- Match case, Whole word and Regex work like Match case, Match whole word only and Regular expression in the Find
  window ([Find, Replace and Go to Line](search-and-replace.md)). A pattern that is not valid gives a message on the
  status line.
- The results list has one row for each page that matches, with the number of matches and a short piece of its text.
  The status line shows how many matches were found on how many pages. Click a row to open that page at its first match.
- Every page you open shows the matches of the last search. The current match has the selection colours.
- F3 or **Next** goes to the next match on the page, Shift+F3 or **Previous** to the one before. The status line says
  which match you are on, for example "2 of 7 matches", and says when the search wraps around the page.
- To remove the highlights, clear the box and press Enter. Changing the box without pressing Enter changes nothing.

## Hide and Show
**Hide** hides the side pane and the page takes its space; the button then reads **Show** and gives the pane back. The
window keeps its size. Ctrl+F shows a hidden pane again. Every new window opens with the pane shown.
