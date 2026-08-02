# C++ Sworddeck + SwordFM

C++ rewrites of the Python cyberdeck and a new Thunar-like file manager.

## Binaries

| Binary | Size | Location |
|--------|------|----------|
| `sworddeck` | 696 KB | `~/bin/sworddeck` |
| `swordfm` | 506 KB | `~/bin/swordfm` |

## Build

```bash
./install-cpp.sh
```

Or manually:

```bash
cmake -B cpp-sworddeck/build -S cpp-sworddeck -DCMAKE_BUILD_TYPE=Release
cmake --build cpp-sworddeck/build -j$(nproc)

cmake -B cpp-filemanager/build -S cpp-filemanager -DCMAKE_BUILD_TYPE=Release
cmake --build cpp-filemanager/build -j$(nproc)
```

## Run

```bash
sworddeck              # Animated desktop HUD (replaces python cyberdeck.py)
sworddeck --screen 2560x1440  # Override resolution

swordfm                # File manager (Thunar-like)
swordfm /home          # Open at specific path
```

## Keybinds (sworddeck)

Same as the Python version:
- `Super+Return` — terminal
- `Super+d` — launcher
- `Super+Ctrl+6` — restart deck
- `Super+Ctrl+e` — edit graph
- `Super+Shift+y` — file manager

## SwordFM Features

- Directory tree sidebar with bookmarks
- List/icon view toggle
- Back/forward/up navigation
- Path bar (Ctrl+L to focus)
- Search filter
- Right-click context menu (open, rename, delete, properties, open in terminal)
- Multi-select with Ctrl/Shift
- Copy/cut/paste (Ctrl+C/X/V)
- Drag and drop
- Keyboard shortcuts (F2 rename, Delete, F5 refresh, Ctrl+N new folder)

## Dependencies

- Qt6 (Widgets, Gui)
- libxcb, libx11
- CMake 3.20+
