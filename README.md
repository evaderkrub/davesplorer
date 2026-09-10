# Davesplorer

A native C++20 file manager for Windows, Linux and macOS, built on SDL3 and
Dear ImGui (docking branch), with a Windows Explorer style interface. Windows
builds are portable folders; Linux and macOS builds use the desktop’s
filesystem, trash, and application associations.

## Linux

On Debian/Ubuntu, install the build dependencies if they are missing:

```sh
sudo apt install build-essential cmake ninja-build git pkg-config libglib2.0-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev \
  libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev libdecor-0-dev \
  libegl1-mesa-dev libgl1-mesa-dev wayland-protocols
```

Use CMake 3.24+ and a compiler supporting C++20. The first configure downloads
SDL3, Dear ImGui, and the test engine; subsequent builds reuse them.

```sh
./build.sh                         # configure, build, and run both test suites
./build/linux-release/stage/davesplorer
./build/linux-release/stage/davesplorer "$HOME/Downloads"
./build.sh debug                   # optional debug build
```

The equivalent manual commands are `cmake --preset linux-release`,
`cmake --build --preset linux-release`, and
`ctest --preset linux-release --output-on-failure`.

Both Wayland and X11 are supported. Assets are staged beside the executable;
settings and the dock layout live in `$XDG_CONFIG_HOME/davesplorer`
(default `~/.config/davesplorer`). Home and special folders follow the
Linux desktop configuration. “This PC” shows the root filesystem and mounted
volumes exposed by GIO.

Linux paths are case-sensitive, use `/`, and preserve spaces and backslashes
in filenames. Delete uses the desktop trash through GIO; permanent deletion
requires the existing confirmation dialog. Copy and move reject existing
destination names without overwriting; copying into the same directory
creates a uniquely named copy. Linux does not yet provide the Windows shell’s
progress and conflict dialogs, and large operations run synchronously.

File cut/copy/paste offers GNOME and KDE clipboard formats and URI lists.
Incoming file drops and dragging between Davesplorer views are supported.
**Dragging out to another application is not implemented on Linux yet; use
copy/paste instead.** The tests use SDL’s dummy video driver so clipboard
checks do not alter your desktop clipboard. External desktop interoperability
still depends on the receiving application.

“Open in system file manager” uses the desktop’s folder handler; revealing
an item uses `org.freedesktop.FileManager1` with a folder-opening fallback.
“Open terminal here” finds an installed terminal launcher. The Windows
registration script and Explorer interceptor apply only to Windows.

## What it does (Windows feature reference)

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
- **Thumbnails view**: **View > Thumbnails** (or the grid button on the
  toolbar) lays the folder out as a grid, Explorer's icon views. PNG, JPEG,
  BMP, GIF, TGA, PSD, HDR and PNM files show a real thumbnail, decoded on a
  background thread so a folder of big photos never stalls; everything
  else shows its icon large. **View > Thumbnail size** picks Small, Medium,
  Large or Extra large. Selection, rubber-band, drag and drop, context
  menus and keyboard (arrows move across and down the grid) all work as in
  the details table, and the choice is remembered for new views. Names are
  cut to two lines; the tooltip has the whole one.
- **Image viewer**: double-clicking an image opens it in a file view, a
  dockable window beside the folder view (drag its tab anywhere a folder
  view can go). Wheel zooms about the cursor, dragging pans, double-click
  flips between fit and real pixels, `+`/`-`/`0`/`1` zoom in, out, fit and
  100%, `Left`/`Right` step through the folder's images, `Ctrl+W` closes.
  The toolbar jumps the folder view to the file or opens it with the
  default program. **View > Open images in Davesplorer** turns the
  double-click behaviour off; the context menu's "View in Davesplorer"
  and "Open with default program" are always there. Very large images are
  shown reduced; animated GIFs show their first frame; EXIF rotation is
  not applied.
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

    .\register.ps1                          "Open in Davesplorer" on the right-click menu of folders and drives
    .\register.ps1 -Default -WinE           folders open in Davesplorer; Win+E and the taskbar Explorer button start it
    .\register.ps1 -Default -WinE -Interceptor   also catch apps that launch explorer.exe by name
    .\register.ps1 -Unregister              back to Explorer (everything, interceptor included)

Explorer keeps running as the desktop shell (taskbar, Start menu, desktop);
only the "open a folder" paths are redirected. Davesplorer's own "Show in
Windows Explorer" and the status bar's Explorer button call `explorer.exe`
directly, so they still open the real thing. The Win+E redirect uses the
shell's File Explorer object verb, a documented per-user override; if a
Windows update ever stops honoring it, PowerToys Keyboard Manager can map
Win+E to `davesplorer.exe` instead.

The verbs above cover programs that ask the shell to open a folder, but not
programs that run `explorer.exe /select,"file"` by name (a common way to
"reveal" a file); nothing in the registry can redirect a launch by exe name.
`-Interceptor` covers those too, system-wide. It installs `explorer_shim.exe`
(built and staged beside `davesplorer.exe`, source in `src/platform`) as the
Image File Execution Options debugger for `explorer.exe`: Windows then runs
the shim in place of every `explorer.exe` launch. The shim reads the
arguments and diverts only what it recognizes as browsing a folder (a folder
path, or `/select` `/e` `/root` naming one) to Davesplorer; everything else,
above all the argument-less launch that is the desktop shell itself, runs the
real Explorer from a private copy, unchanged. Pass-through is the default, so
an unrecognized launch never goes to Davesplorer and the shell can't be lost.

The interceptor catches launches that name `explorer.exe` and so create a
new process. `-Default` catches "open this folder" requests that resolve the
folder's handler instead: it sets both the folder's default verb (double
click) and its `open` verb command (what apps request with
`ShellExecute("open", folder)`), and blanks the `open` DDE entry so the
folder is not handed to the running desktop Explorer. A program that was
already running when you registered may have cached the old handler:
`register.ps1` broadcasts an association-change notification to refresh
them, but a few only re-read on restart.

This is admin-level and system-wide (it writes HKLM), and it is the same
registry mechanism some malware uses to hijack the shell, so Defender or an
antivirus may flag it. `register.ps1 -Interceptor` re-elevates itself for the
one write. Recovery, should the desktop ever fail to appear: Ctrl+Shift+Esc
for Task Manager, run `powershell`, then `register.ps1 -Unregister`.

The shim has a dry-run mode for inspecting its routing without launching
anything: set `DSP_SHIM_DRYRUN=1` and it appends each decision to
`%TEMP%\dsp_shim.log` instead. For example:

    $env:DSP_SHIM_DRYRUN=1
    & .\explorer_shim.exe "C:\Windows\explorer.exe" "C:\Users"          # -> DAVESPLORER
    & .\explorer_shim.exe "C:\Windows\explorer.exe" shell:MyComputerFolder  # -> EXPLORER
    & .\explorer_shim.exe "C:\Windows\explorer.exe"                     # -> EXPLORER (the shell)
    Get-Content $env:TEMP\dsp_shim.log

## Layout

    CMakeLists.txt
    CMakePresets.json     release / debug presets, build dirs under C:/buildfiles/davesplorer
    build.cmd             finds Visual Studio, then configure + build + test
    src/main.cpp          entry point, nothing else
    src/app/              application state and logic, no ImGui
    src/ui/               everything that draws
    src/platform/         Win32 platform layer and shared SDL host loop
    src/platform/linux/   Linux filesystem, clipboard, and desktop integration
    src/platform/macos/   macOS equivalents, in Objective-C++
    assets/fonts/         Open Sans, Fira Code, Material Icons
    assets/icon/          the red-folder app icon (.ico, .png)
    tools/make_icon.py    regenerates the icon
    tests/                console test binaries
    cmake/dsp_imconfig.h  ImGui configuration (test-engine hooks on)

The interface reads application state and owns none of it. `app::AppState`
and `app::Tab` are plain structs; the unit tests exercise navigation,
listing, sorting, selection and file operations without a window. Failures
cross module boundaries as `bool` plus an error string, never as exceptions.

## macOS

Requires the Xcode command line tools (`xcode-select --install`) plus CMake
3.24+ and Ninja; with Homebrew, `brew install cmake ninja`. Nothing else: the
platform layer is Objective-C++ against Foundation, AppKit and
UniformTypeIdentifiers, which ship with the system. The first configure
downloads SDL3, Dear ImGui, and the test engine.

```sh
./build.sh                         # configure, build, and run both test suites
./build/macos-release/stage/davesplorer
./build/macos-release/stage/davesplorer "$HOME/Downloads"
./build.sh debug                   # optional debug build
```

`build.sh` picks the preset from `uname`; the manual commands are
`cmake --preset macos-release`, `cmake --build --preset macos-release`, and
`ctest --preset macos-release --output-on-failure`. SDL renders through
Metal.

Assets are staged beside the executable; settings and the dock layout live in
`~/Library/Application Support/davesplorer`. Home and special folders come
from the standard search paths, with Movies in place of Videos. “This PC”
shows the root volume and everything mounted under `/Volumes`, named the way
Finder names it. Items are hidden by a leading dot or by the filesystem’s
own hidden flag. Type names come from UniformTypeIdentifiers.

Delete moves to the Finder trash through `NSFileManager`; permanent deletion
requires the existing confirmation dialog. Rename and move are atomic and
never overwrite (`renamex_np` with `RENAME_EXCL`). “Open” and “Reveal” hand
off to `NSWorkspace`, and “Open terminal here” launches Terminal.app.

The default APFS volume folds case, so two names differing only in case are
one item; the tests detect this rather than assume it.

Cut/copy/paste goes through `NSPasteboard` rather than SDL, which on macOS
would publish a custom type as a synthesized `dyn.` identifier no other
application knows and would read back only the first pasteboard item. Files
copied here paste in Finder and files copied in Finder paste here, all of
them. Each file is written as a `public.file-url` item; the paths also ride
along verbatim in a private type, because a file URL comes back canonically
decomposed and this app’s own paste wants the bytes it was given. Cut is
private too — nothing on macOS cuts files through the pasteboard, so other
applications see a plain copy. Off the Cocoa driver, which is how the tests
run, the clipboard is a private pasteboard, so a test run leaves whatever
you have copied alone.
**Dragging out to another application is not implemented on macOS yet; use
copy/paste instead.** The build produces a plain executable rather than an
`.app` bundle, so there is no Dock icon or menu bar of its own.

## Windows build

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
