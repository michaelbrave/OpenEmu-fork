# OpenEMU Linux Port Plan (C++/Qt6)

## Context

OpenEMU is a macOS-only multi-emulator frontend.
The goal is to make it run on Linux.
**Pivoted from Swift/GTK4 to C++/Qt6** for better Linux native integration and performance.

---

## Current Status: Phase 6 COMPLETE

- [x] **Phase 0**: Headless C adapters for SNES9x, mGBA, FCEU (NES).
- [x] **Phase 1**: C++ `CoreBridge` class to load cores via dynamic linking (`dlopen`).
  - [x] Define `OECoreInterface` (C function table in `oe_core_interface.h`).
  - [x] Implement `CoreBridge` wrapper in C++.
  - [x] Verify with `qt_test` smoke test.
- [x] **Phase 2**: Database Layer (QtSql / SQLite).
  - [x] `LibraryDatabase` using `QSqlDatabase`.
  - [x] Schema: Systems, Games, ROMs, SaveStates, Collections.
  - [x] ROM scanning with `computeMd5` (QCryptographicHash).
  - [x] Metadata extraction via `guessSystemIdentifier`.
  - [x] 35/35 tests passing in `db_test`.
- [x] **Phase 3**: Input & Audio (SDL2 / Qt).
  - [x] SDL2 for low-latency audio output (queue capped at ~50ms to prevent latency buildup).
  - [x] SDL2 for game controller and keyboard mapping.
  - [x] System responder logic in C++.
- [x] **Phase 4**: Video & Game Loop (Qt OpenGL).
  - [x] `EmulatorView` using `QOpenGLWidget`.
  - [x] RGB565 and RGBA8888 texture uploads.
  - [x] 60fps sync'd game loop in background thread.
  - [x] Basic shader support (GLSL).
- [x] **Phase 5**: Library UI (Qt Widgets).
  - [x] `MainWindow` with `QSplitter` (Sidebar | Game Grid).
  - [x] `Sidebar` with LIBRARY / CONSOLES section headers listing available systems.
  - [x] `GameGridView` (using `QListView`) for game covers, double-click to launch.
  - [x] ROM import flow (drag-and-drop + File menu).
  - [x] Game selection launches emulator window with correct core.
  - [x] System-specific rounded cover art tiles with sheen highlight (NES=red, SNES=purple, GBA=green, etc.).
  - [x] `CoverArtDownloader` with local cover art lookup support.
  - [x] Dark charcoal theme close to original OpenEMU aesthetic (#1c1c1e, #0a84ff accent).
- [x] **Phase 6**: Preferences & Polish.
  - [x] Preferences dialog (ROM Paths, Cover Art API key, Emulation settings).
  - [x] Controller mapping dialog — live SDL capture, per-button keyboard/controller remapping, saved to QSettings.
  - [x] Edit Game Info dialog (rename title, change system).
  - [x] Cover Art dialog (search and set art per game).
  - [x] TheGamesDB API integration.
  - [ ] Save state gallery UI — not built.
  - [ ] Packaging (AppImage / Flatpak) — not built.

---

## Roadmap

### Phase 7: Multi-Process Core Isolation
**Goal:** Each emulator core runs in a child process. A crash in a core cannot bring down the UI.

- [ ] Fork/exec a `oe_core_host` child process per game launch.
- [ ] IPC channel (Unix socket or shared memory) for frame data, audio, and input.
- [ ] `CoreBridge` talks to the child process instead of directly calling the .so.
- [ ] Graceful handling of core crash (show error in UI, allow relaunch).

### Remaining Open Issues
- **SNES**: Live end-to-end test needed to confirm the stride fix and button mappings work with a real ROM.
- **GBA**: PS4 L1/R1 may need remapping via the Controller Mapping dialog if the SDL controller DB entry is wrong for your specific pad.
- **NES**: Controls "not perfect" — further investigation needed. Likely a button-layout expectation (NES only has A/B; X/Y/L/R silently no-op which is correct).
- **Library view modes**: Configurable grid / list / large-icon views (Finder-style) — not yet built.

---

## Architecture

```
openemu-linux (Qt6 Widgets app)
├── MainWindow          oe_library_ui.cpp       Library browser UI
├── Sidebar             oe_library_ui.cpp       System list with section headers
├── GameGridView        oe_library_ui.cpp       Icon-mode game grid, cover art
├── EmulatorView        oe_emulator_view.cpp    QOpenGLWidget rendering core frames
├── GameLoop            oe_game_loop.cpp        60fps QThread: input→runFrame→audio
├── CoreBridge          oe_core_bridge.cpp      dlopen wrapper for core .so files
├── LibraryDatabase     oe_library_database.cpp Qt SQL / SQLite
├── InputBackend        oe_input_backend.cpp    SDL2 keyboard + gamepad polling
├── AudioBackend        oe_audio_backend.cpp    SDL2 audio queue push
├── PreferencesDialog   oe_preferences_dialog.cpp
├── ControllerMappingDialog oe_controller_mapping_dialog.cpp  Live SDL button capture
├── CoverArtDownloader  oe_cover_art.cpp        Local cache + TheGamesDB API
└── Cores (.so files)
    ├── liboe_core_mgba.so    mGBA (GBA / GB / GBC)
    ├── liboe_core_snes9x.so  SNES9x (SNES)
    └── liboe_core_fceu.so    FCEU Ultra (NES)
```

## Key Files

| File | Purpose |
|------|---------|
| `linux/src/main.cpp` | Entry point, wires up all components |
| `linux/src/oe_library_ui.cpp` | Main window, sidebar, game grid |
| `linux/src/oe_emulator_view.cpp` | OpenGL frame rendering |
| `linux/src/oe_game_loop.cpp` | 60fps emulation thread |
| `linux/src/oe_core_bridge.cpp` | Core .so loader |
| `linux/src/oe_library_database.cpp` | SQLite game library |
| `linux/src/oe_audio_backend.cpp` | SDL2 audio output |
| `linux/src/oe_input_backend.cpp` | SDL2 input with custom mappings |
| `linux/src/oe_controller_mapping_dialog.cpp` | Button remapping UI |
| `linux/src/oe_cover_art.cpp` | Cover art download + cache |
| `linux/src/oe_preferences_dialog.cpp` | Preferences UI |
| `linux/cores/snes9x/snes9x_adapter.cpp` | SNES9x core adapter |
| `linux/cores/mgba/mgba_adapter.c` | mGBA core adapter |
| `linux/cores/fceu/fceu_adapter.cpp` | FCEU NES core adapter |
| `linux/CMakeLists.txt` | Build configuration |
| `plan.md` | This file |
| `thingstofix.md` | Bug tracker |

## Build

```bash
cd linux/
cmake -B build
cmake --build build -j$(nproc)
./build/openemu-linux
```
