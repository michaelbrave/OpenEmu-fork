# OpenEMU Linux Port Plan (Pivoted to C++/Qt6)

## Context

OpenEMU is a macOS-only multi-emulator frontend.
The goal is to make it run on Linux.
**Update March 2026**: Pivoted from Swift/GTK4 to **C++/Qt6** for better Linux native integration and performance.

---

## Status: Phase 5 (Library UI) COMPLETE
- [x] **Phase 0**: Headless C adapters for SNES9x, mGBA, FCEU (NES).
- [x] **Phase 1**: C++ `CoreBridge` class to load cores via dynamic linking (`dlopen`).
    - [x] Define `OECoreInterface` (C function table).
    - [x] Implement `CoreBridge` wrapper in C++.
    - [x] Verify with `qt_test` smoke test.
- [x] **Phase 2**: Database Layer (QtSql / SQLite).
    - [x] `LibraryDatabase` using `QSqlDatabase`.
    - [x] Schema: Systems, Games, ROMs, SaveStates, Collections.
    - [x] ROM scanning with `computeMd5` (QCrypographicHash).
    - [x] Metadata extraction via `guessSystemIdentifier`.
    - [x] 35/35 tests passing in `db_test`.
- [x] **Phase 3**: Input & Audio (SDL2 / Qt).
    - [x] SDL2 for low-latency audio output.
    - [x] SDL2 for game controller and keyboard mapping.
    - [x] System Responder logic in C++.
- [x] **Phase 4**: Video & Game Loop (Qt OpenGL).
    - [x] `EmulatorView` using `QOpenGLWidget`.
    - [x] RGB565 and RGBA8888 texture uploads.
    - [x] 60fps sync'd game loop in background thread.
    - [x] Basic shader support (GLSL).
- [x] **Phase 5**: Library UI (Qt Widgets) - COMPLETE.
    - [x] `MainWindow` with `QSplitter` (Sidebar | Game View).
    - [x] `Sidebar` listing available systems.
    - [x] `GameGridView` (using `QListView`) for game covers.
    - [x] ROM Import flow (drag-and-drop).
    - [x] Game selection launches emulator window with core.
    - [x] System-specific colored tile placeholders (NES=red, SNES=purple, GBA=green, etc.)
    - [x] CoverArtDownloader with local cover art lookup support.
    - [x] Dark theme with hover/selection styling (#1a1a2e, #e94560 accent).

---

## Roadmap

### Phase 6: Preferences & Polish
**Milestone:** Full feature-complete Linux application.

- [x] Preferences dialog (ROM Paths, Cover Art, Emulation settings).
- [x] Input mapping tab.
- [ ] Save state gallery UI.
- [x] Cover art API integration (TheGamesDB).
- [ ] Packaging (AppImage / Flatpak).

---

## Key Files

### Core
- `linux/src/oe_library_ui.cpp` - Main UI (MainWindow, Sidebar, GameGridView)
- `linux/src/oe_cover_art.cpp` - Cover art downloader with local lookup
- `linux/src/oe_games_api.cpp` - TheGamesDB API integration
- `linux/src/oe_preferences_dialog.cpp` - Preferences dialog with tabs
- `linux/src/main.cpp` - Entry point
- `linux/CMakeLists.txt` - Build config

### Documentation
- `LINUX_PORT_PLAN.md` - This file
