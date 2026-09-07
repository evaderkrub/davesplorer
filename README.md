# Davesplorer

A Windows File Explorer clone: a native C++20 desktop application built on
SDL3 and Dear ImGui (docking branch). The staged build is one folder that can
be copied to another Windows machine and run; nothing needs installing.

## What it does

- **Browse** drives and folders with a Windows Explorer layout: navigation
  pane on the left (Quick access, This PC, expandable drive tree), tabbed file
  list on the right, status bar underneath. The panes dock and can be
  rearranged; **View > Reset layout** restores the default.
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
- **Tabs**: `Ctrl+T` new, `Ctrl+W` close, `Ctrl+Tab` cycle, middle-click to
  close, `+` button on the tab bar. Closing the last tab exits.
- **File operations**: New folder (`Ctrl+Shift+N`), new text file, Rename
  (`F2`), Delete to Recycle Bin (`Del`), Delete permanently (`Shift+Del`),
  Cut/Copy/Paste (`Ctrl+X`/`C`/`V`) between folders and tabs, Copy as path.
  Copy and move go through the Windows shell, so its progress and conflict
  dialogs appear exactly as they do in Explorer.
- **Context menus** on items and on the empty area, plus "Show in Windows
  Explorer" and "Open terminal here".
- **View options**: hidden items, file name extensions, navigation pane,
  status bar.
- **Scaling**: `Ctrl++` / `Ctrl+-` / `Ctrl+0` or `Ctrl`+wheel zoom the whole
  interface, independently of the monitor's DPI scale, which is also honored.
- **Themes**: Wili Dark (default), Slate Dark, Light, Midnight, Nord. Styles,
  fonts (Open Sans, Fira Code) and Material Design icons come from fwcom.
- **About** box under Help, modal like every dialog here.

Settings (theme, zoom, view options, window size, last folder) persist in
`settings.ini` beside the executable; the dock layout persists in `imgui.ini`
beside it too. Every runtime path is resolved from the executable's own
directory, never the working directory.

## Layout

    CMakeLists.txt
    CMakePresets.json     release / debug presets, build dirs under C:/buildfiles/davesplorer
    build.cmd             finds Visual Studio, then configure + build + test
    src/main.cpp          entry point, nothing else
    src/app/              application state and logic, no ImGui
    src/ui/               everything that draws
    src/platform/         Win32 file system and shell, SDL host loop
    assets/fonts/         Open Sans, Fira Code, Material Icons
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
  zooms, switches themes, opens and closes tabs, and creates, renames,
  deletes and copy-pastes files through the dialogs and shortcuts.

Both are registered with CTest; `build.cmd release test` runs them.
