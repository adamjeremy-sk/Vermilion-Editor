# Requirement: directory sidebar (right-hand side)

**Status: kept by Adam (2026-11-05).** Derived from the behaviour of the CrimsonEditor panel, written down before that code was deleted; the code itself is not used. In Vermilion the sidebar sits on the **right**, as the first tab of the right-hand stack (Files | Agent). References below to legacy function names describe behaviour only.
Sources: `FileWnd.cpp` (531 lines), `FileWndDirectory.cpp` (1,247 lines), IDR_FILE_WINDOW menu and accelerators, `MainFrmHndr.cpp` (Sync).

### 1 Layout
- Legacy panel had two category tabs, Directory and Project. Project tab DROPPED.
- Small toolbar: Refresh | separator | Copy To | Move To | Delete. Panel header buttons: Sync, Hide.
- Below: a "local drive" dropdown (roots) and a file-filter dropdown, then the tree.
- One tree shows folders AND files (no separate list pane). Folders first, then files; each group sorted alphabetically.

### 2 Behaviour
- Lazy loading: a folder gets a placeholder child only if it has children (`DirectoryHasChildren`); real children are inserted on expand (`InsertDirectoryTreeChildren`).
- Hidden files and folders are never shown in legacy. Vermilion: dotfiles hidden by default with a toggle (decision 3).
- File filter dropdown selects one of the user-defined filters (description, extension list, default extension). Filters apply to files only; folders always show. Default "*.*". Filters come from preferences (Filters page).
- Selecting any item makes its directory the process's current directory (`SetAsCurrentDirectory`; for a file, its parent). Relative paths in user tools and Find in Files use it. "Set as Work Dir" is a separate explicit command (`SetAsWorkingDirectory`, persisted).
- Double-click: file opens in the editor (posted open-document request); folder expands/collapses only; an unreachable item reports an error (`OpenDirectoryItem`). Single click does nothing extra.
- Sync button: navigates the tree to the active document's path (`SetBrowsingDirectory`). Not available for new unsaved or remote documents.
- Refresh (F5, button, menu): re-reads the selected node only (`RefreshDirectoryItem`).
- Panel-local keys: Enter open, F2 rename, Delete delete, F5 refresh. Suspended while a label is being edited.
- Inline rename by label editing.
- Drag and drop within the tree to move or copy (`OnBegindragDirectoryTree`, `OnDragOverDirectoryTree`, `OnDropDirectoryTree`).
- Visibility toggled from View menu (Alt+Shift+1) and the panel's Hide button.
- Browsing directory and working directory are remembered across sessions.

### 3 Context menus (chosen by what is under the cursor; right-click selects the item first)
- Empty area: Refresh, Hide.
- Root: Explore, Find, New > (Folder, Text Document), Set as Work Dir, Properties, Refresh, Hide.
- Folder: Explore, Find, New > (Folder, Text Document), Set as Work Dir, Copy To, Move To, Delete, Rename, Properties, Refresh, Hide.
- File: Open, Shell Execute, Copy To, Move To, Delete, Rename, Properties, Refresh, Hide.
Linux mapping: Explore = open in file manager; Find = Find in Files rooted at the folder; Shell Execute = open with default app (GIO); Properties = small dialog (name, size, modified, permissions); Delete = trash by default (`g_file_trash`) with confirmation; Copy To/Move To = folder chooser then GIO copy/move.

### 4 Windows-only parts to drop or replace
Drive letters and drive icons, `SHGetFileInfo` icons and localised display names, `SHFileOperation`, `ShellExecute("find")`, backslash paths, MAX_PATH handling. Replace with GIO and roots: Home, Desktop, filesystem root, mounted volumes (GVolumeMonitor).

### 5 Suggested GTK 4 mapping (proposal)
`GtkTreeListModel` + `GtkTreeExpander` in a `GtkListView`/`GtkColumnView`, `GFileEnumerator` for async loading, `GFileMonitor` for auto-refresh (new), `GtkDragSource`/`GtkDropTarget`, `GtkPopoverMenu`, `GtkFilter`. Filesystem work lives in core behind a small `FileOps` interface with no GTK, so it is unit-testable.

### 6 Acceptance tests to write for E6
Lazy expansion; folders-first sort; dotfiles hidden and toggle; filter applies to files only; double-click opens file or expands folder; sync follows the active tab; rename/delete/copy/move including failure paths; current directory follows selection; remembered browsing directory restored.
