# Davesplorer

A Windows File Explorer clone: a native C++20 desktop application built on
SDL3 and Dear ImGui (docking branch). The staged build is one folder that can
be copied to another Windows machine and run; nothing needs installing.

## What it does

- **Browse** drives and folders with a Windows Explorer layout: navigation
  pane on the left (Quick access, This PC, expandable drive tree), one or
  more folder views on the right, status bar underneath each. Everything
  docks and can be rearranged; **View > Reset layout** restores the default.
- **Shortcut bar** across the top: ten slots, each showing a red `+` until
  used. Clicking an empty slot pins the selected folder (or the folder being shown when
  nothing is selected); from then on the slot jumps the active view there,
  Ctrl+click opens it in a new view, and files can be dropped on it.
  Right-click a slot for Open, Open in new view, Assign, or Clear. Slots
  persist in `settings.ini`; **View > Shortcut bar** hides the bar.
- **Address bar** with clickable breadcrumbs. Click the empty part (or
  `Ctrl+L`) to type a path. Deep paths collapse into a `...` menu.
- **Search** box filters the current folder as you type (`Ctrl+F`).
- **Details view** with Name, Date modified, Type and Size columns. Click a
  header to sort; folders stay first. Columns resize, reorder and hide.
- **Selection** like Explorer: click, `Ctrl`+click, `Shift`+click, `Ctrl+A`,
  arrow keys, `Home`/`End`, `Page Up`/`Page Down`.
- **Open** with double-click or `Enter`: folders navigate, files launch in
  their associated program.
- **History**: Back/Forward/Up buttons, `Alt+Left`/`Alt+Right`/`Alt+Up`,
  `Backspace` for back, `F5` to refresh.
- **Views**: every open location is its own dockable window, titled after
  its folder. New views (`Ctrl+T`, the `+` toolbar button, "Open in new
  view") appear as tabs beside the active view; drag a tab out to dock it
  beside, above or below another, or float it. **View > Split view right /
  down** (`Ctrl+Shift+Right` / `Ctrl+Shift+Down`, or the toolbar buttons)
  opens a second view of the current folder next to it in one step, for a
  dual-pane layout. `Ctrl+Tab` cycles views, `Ctrl+W` closes the active one,
  and closing the last view exits. The layout is remembered.
- **File operations**: New folder (`Ctrl+Shift+N`), new text file, Rename
  (`F2`), Delete to Recycle Bin (`Del`), Delete permanently (`Shift+Del`),
  Copy as path. Copy and move go through the Windows shell, so its progress
  and conflict dialogs appear exactly as they do in Explorer.
- **Cut, copy and paste** (`Ctrl+X`/`C`/`V`) use the real Windows clipboard
  (`CF_HDROP` plus the preferred drop effect), so files copied in Explorer
  paste here and files copied here paste in Explorer. Cut items show ghosted
  until pasted; a cut+paste moves and then empties the clipboard.
- **Drag and drop**: drag selected items onto a folder row, the empty part
  of the list, a folder in the navigation pane, another view, or a breadcrumb.
  Same drive moves, another drive copies; `Ctrl` forces copy, `Shift` forces
  move, and the drag tooltip says which. A drag parked over a tab or a closed
  tree node opens it. Files dragged in from Explorer or any other program
  land in the folder under the cursor; dragging out of the window hands the
  items to Windows, so they can be dropped on Explorer, the desktop, or an
  editor.
- **Context menus** on items and on the empty area, plus "Show in Windows
  Explorer" and "Open terminal here". Each view's status bar also has New
  folder, Open terminal and Open in Windows Explorer buttons at its right
  end.
- **Command line**: `davesplorer.exe <folder>` opens that folder;
  `davesplorer.exe <file>` opens the file's folder with the file selected.
  Explorer's own switches work too (`/select,"path"`, `/e,folder`), so a
  program that runs `explorer.exe /select,...` can run `davesplorer.exe`
  with the same arguments.
- **View options**: hidden items, file name extensions, navigation pane,
  status bar.
- **Scaling**: `Ctrl++` / `Ctrl+-` / `Ctrl+0` or `Ctrl`+wheel zoom the whole
  interface, independently of the monitor's DPI scale, which is also honored.
- **Themes**: Wili Dark (default), Slate Dark, Light, Midnight, Nord. Styles,
  fonts (Open Sans, Fira Code) and Material Design icons come from fwcom.
- **About** box under Help, modal like every dialog here.
- **App icon**: a red folder, embedded as a Windows resource (Explorer, the
  taskbar and the window title bar all show it) along with version info.
  `tools/make_icon.py` redraws `assets/icon/davesplorer.ico` and `.png`
  with Pillow.

Settings (theme, zoom, view options, window size, last folder) persist in
`settings.ini` beside the executable; the dock layout persists in `imgui.ini`
beside it too. Every runtime path is resolved from the executable's own
directory, never the working directory.

## Using it instead of Explorer

`register.ps1` (staged beside the exe, source in `tools/`) wires Davesplorer
into the shell for the current user only; nothing needs administrator
rights and `-Unregister` puts everything back.

    .\register.ps1                  "Open in Davesplorer" on the right-click menu of folders and drives
    .\register.ps1 -Default -WinE   folders open in Davesplorer; Win+E and the taskbar Explorer button start it
    .\register.ps1 -Unregister      back to Explorer

Explorer keeps running as the desktop shell (taskbar, Start menu, desktop);
only the "open a folder" paths are redirected. Davesplorer's own "Show in
Windows Explorer" and the status bar's Explorer button call `explorer.exe`
directly, so they still open the real thing. The Win+E redirect uses the
shell's File Explorer object verb, a documented per-user override; if a
Windows update ever stops honoring it, PowerToys Keyboard Manager can map
Win+E to `davesplorer.exe` instead.

## Layout

    CMakeLists.txt
    CMakePresets.json     release / debug presets, build dirs under C:/buildfiles/davesplorer
    build.cmd             finds Visual Studio, then configure + build + test
    src/main.cpp          entry point, nothing else
    src/app/              application state and logic, no ImGui
    src/ui/               everything that draws
    src/platform/         Win32 file system and shell, SDL host loop
    assets/fonts/         Open Sans, Fira Code, Material Icons
    assets/icon/          the red-folder app icon (.ico, .png)
    tools/make_icon.py    regenerates the icon
    tests/                console test binaries
    cmake/dsp_imconfig.h  ImGui configuration (test-engine hooks on)

The interface reads application state and owns none of it. `app::AppState`
and `app::Tab` are plain structs; the unit tests exercise navigation,
listing, sorting, selection and file operations without a window. Failures
cross module boundaries as `bool` plus an error string, never as exceptions.

## Build

Requirements: Visual Studio 2022 or later with the C++ workload, CMake 3.24+,
Ninja, and network access on the first configure (SDL3, Dear ImGui and the
ImGui Test Engine are fetched with FetchContent and linked statically, as is
the C runtime).

    build.cmd                 release: configure, build, run tests
    build.cmd debug           same for the debug preset
    build.cmd release build   just build

Output:

    C:\buildfiles\davesplorer\release\stage\     davesplorer.exe + assets\   (copy this folder)
    C:\buildfiles\davesplorer\release\tests\     davesplorer_unit_tests.exe, davesplorer_e2e_tests.exe

Project code compiles with `/W4 /WX /permissive-`.

## Tests

- `davesplorer_unit_tests` covers path handling, number formatting, settings
  round-trips, navigation history, listing/sort/filter, selection rules and
  file operations against a scratch folder under `%TEMP%`.
- `davesplorer_e2e_tests` runs the real interface headless under the Dear
  ImGui Test Engine: it types into the address bar, double-clicks folders,
  uses Back/Forward/Up and the breadcrumbs, filters with the search box,
  selects with Ctrl/Shift, sorts by column header, opens the About modal,
  zooms, switches themes, opens, cycles and closes views, splits a view and
  drags a file from one side to the other, creates, renames, deletes and
  cut/copy-pastes files through the dialogs and shortcuts, drags rows onto
  folders, breadcrumbs and the navigation tree, and simulates a drop
  arriving from another program.
  `davesplorer_e2e_tests <name-filter> -v` runs one test with a debug log.

The unit tests exercise the real Windows clipboard and put any text that
was on it back afterwards.

Both are registered with CTest; `build.cmd release test` runs them.

## License

Davesplorer is released under the MIT License (see `LICENSE`). Bundled
third-party assets keep their own licenses, each beside the files in
`assets/fonts`: Open Sans and Fira Code under the SIL Open Font License 1.1,
Material Icons under the Apache License 2.0. SDL3, Dear ImGui and the ImGui
Test Engine are fetched at build time under their respective zlib and MIT
licenses.
