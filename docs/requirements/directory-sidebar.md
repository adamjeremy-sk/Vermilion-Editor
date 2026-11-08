# Requirement: directory sidebar (right-hand side)

**Status: kept by Adam (2026-11-05).** In Vermilion the sidebar sits on the **right**, as the first tab of the right-hand stack (Files | Agent | CTXMon). Built in milestone M3.

### 1 Layout
- One tab, Files. There is no separate project tab.
- Small toolbar: Refresh | separator | Copy To | Move To | Delete. Panel header buttons: Sync, Hide.
- Below: a roots dropdown and a file-filter dropdown, then the tree.
- One tree shows folders AND files (no separate list pane). Folders first, then files; each group sorted alphabetically.

### 2 Behaviour
- Lazy loading: a folder gets a placeholder child only if it has children; real children are inserted on expand.
- Dotfiles (hidden files and folders) are hidden by default, with a toggle to show them.
- The file-filter dropdown selects one of the user-defined filters (description, extension list, default extension). Filters apply to files only; folders always show. Default "*.*". Filters come from the settings.
- Selecting any item makes its directory the current directory (for a file, its parent). Find in Files uses it. "Set as Work Dir" is a separate explicit command, and the working directory is persisted.
- Double-click: a file opens in the editor; a folder expands or collapses only; an unreachable item reports an error. Single click does nothing extra.
- Sync button: navigates the tree to the active document's path. Not available for new unsaved documents.
- Refresh (F5, button, menu): re-reads the selected node only.
- Panel-local keys: Enter open, F2 rename, Delete delete, F5 refresh. Suspended while a name is being edited.
- Inline rename.
- Drag and drop within the tree to move or copy.
- Visibility toggled from the View menu (Alt+Shift+1) and the panel's Hide button.
- Browsing directory and working directory are remembered across sessions.

### 3 Context menus (chosen by what is under the cursor; right-click selects the item first)
- Empty area: Refresh, Hide.
- Root: Open in File Manager, Find in Files, New > (Folder, Text Document), Set as Work Dir, Properties, Refresh, Hide.
- Folder: Open in File Manager, Find in Files, New > (Folder, Text Document), Set as Work Dir, Copy To, Move To, Delete, Rename, Properties, Refresh, Hide.
- File: Open, Open With Default App, Copy To, Move To, Delete, Rename, Properties, Refresh, Hide.
Details: Find in Files is rooted at the folder; Open With Default App uses GIO; Properties is a small dialog (name, size, modified, permissions); Delete moves to the trash (`g_file_trash`) after a confirmation; Copy To and Move To use a folder chooser, then a GIO copy or move.

### 4 Roots
Home, Desktop, the filesystem root and mounted volumes (`GVolumeMonitor`). Paths go through GIO; no string-built separators.

### 5 Suggested GTK 4 mapping (proposal)
`GtkTreeListModel` + `GtkTreeExpander` in a `GtkListView`/`GtkColumnView`, `GFileEnumerator` for async loading, `GFileMonitor` for auto-refresh, `GtkDragSource`/`GtkDropTarget`, `GtkPopoverMenu`, `GtkFilter`. Filesystem work lives in core behind a small `FileOps` interface with no GTK, so it is unit-testable.

### 6 Acceptance tests (M3)
Lazy expansion; folders-first sort; dotfiles hidden and toggle; filter applies to files only; double-click opens file or expands folder; sync follows the active tab; rename/delete/copy/move including failure paths; current directory follows selection; remembered browsing directory restored.
