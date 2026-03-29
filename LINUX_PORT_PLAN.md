# OpenEMU Linux Port Plan

## Context

OpenEMU is a macOS-only multi-emulator frontend (167 Swift files, 70 ObjC files, 30+ C/C++ emulator cores).
The goal is to make it run on Linux. The project has zero cross-platform code in the frontend layer —
every subsystem uses a macOS-exclusive API. This plan outlines a phased approach from current state to
a working, packageable Linux application.

---

## Scope Reality Check

| Layer | macOS API | Linux Replacement |
|---|---|---|
| UI | AppKit (NSView, NSWindow, NSApplication) | GTK4 via Adwaita-swift/SwiftGTK |
| Persistence | CoreData (NSManagedObjectContext) | GRDB.swift (SQLite) |
| Game rendering | Metal + CALayer (OEGameLayerView) | OpenGL via GtkGLArea |
| Audio | CoreAudio (OEAudioDeviceManager) | SDL2 audio |
| Input/HID | IOKit (OEDeviceManager, OEHIDEvent) | SDL2 GameController API |
| Build system | Xcode (.xcworkspace) | CMake + Swift Package Manager |
| ObjC runtime | System ObjC | Rewrite all ObjC in Swift (GNUstep not viable) |
| Power mgmt | IOKit.pwr_mgt (IOPMAssertionID) | D-Bus org.freedesktop.ScreenSaver |
| Keyboard | Carbon.HIToolbox.Events | SDL2 keyboard events |
| Updates | Sparkle | Simple GitHub releases JSON check |
| Archive extraction | XADMaster.framework | libarchive or system 7z/unzip |

**Portable already:** All 30+ C/C++ emulator cores. Swift Foundation (works on Linux).
Swift on Linux requires Swift 5.9+.

**ObjC strategy:** Rewrite in Swift. GNUstep cannot provide IKImageBrowserView, NSDocument,
or QuartzCore. The 44 system responder .m files are mechanical (70-80 lines each) and safe to
batch-convert. The large ObjC UI files (OEGridView.m + IKImageBrowserView) must be replaced
wholesale anyway — no incremental path exists.

---

## Phase 0: Build System + Core Compilation on Linux
**Milestone:** 3 emulator cores (SNES9x, mGBA, FCEU) build as `.so` files on Linux and produce
pixel frames in a headless C test harness.

- Set up CMake in `linux/` replacing the Xcode project
- Initialize submodules for 3 representative cores (already done: SNES9x, mGBA, FCEU, OpenEmu-SDK)
- Remove macOS SDK includes from each core's wrapper layer
- Write C/C++ adapters for each core (bypassing ObjC wrapper layer entirely)
- Write a minimal C test harness: init → loadROM → runFrame → write PPM

**Verification:** `cmake --build linux/build && ./linux/build/test_harness rom.sfc` → writes `frame.ppm`

**Complexity:** Medium | **Duration:** 2-4 weeks

---

## Phase 1: Core C/Swift Abstraction Interface
**Milestone:** Swift test binary loads an SNES ROM, runs 60 frames, writes last frame to disk.

- Define `struct OECoreInterface` (pure C function table): init, loadROM, runFrame, saveState,
  loadState, pressButton, releaseButton, moveAxis, destroy
  - Reference: `libretro.h` from RetroArch for design guidance
- Write SNES9x adapter implementing the C interface
- Write Swift `CoreBridge` class wrapping the C function table
- Rewrite the 44 system responder `.m` files in Swift
  - Pattern: `pressEmulatorKey` / `releaseEmulatorKey` / `changeAnalogEmulatorKey`
  - Files: `OpenEmu/SystemPlugins/*/OE*SystemResponder.m`

**Critical file:** `OpenEmu/SystemPlugins/PlayStation/OEPSXSystemResponder.m`

**Complexity:** High | **Duration:** 3-6 weeks

---

## Phase 2: Database Layer (CoreData → GRDB)
**Milestone:** `swift run oelib-test` creates library, adds games, queries by system, manages save
states — entirely on Linux, zero macOS imports.

- Add GRDB.swift via Swift Package Manager (no macOS dependencies)
- Translate the Core Data schema (9 entities) to GRDB `Record` types + SQL migrations
  - Schema: `OpenEmu/OEDatabase.xcdatamodeld/`
  - Entities: Game, ROM, System, SaveState, Screenshot, Image, Collection, CollectionFolder, SmartCollection
- Rewrite these files replacing all NSManagedObjectContext with GRDB:
  - `OpenEmu/OELibraryDatabase.swift` (769 lines)
  - `OpenEmu/OEDBGame.swift`
  - `OpenEmu/OEDBRom.swift`
  - `OpenEmu/OEDBSaveState.swift` (567 lines)
  - `OpenEmu/OEDBSystem.swift`
  - `OEDBCollection`, `OEDBScreenshot`, `OEDBImage`
- Rewrite `ALIterativeMigrator.m` in Swift as a GRDB migration runner
- Rewrite `ROMImporter.swift` — strip AppKit, keep logic, replace NSManagedObject with GRDB records

**Complexity:** High | **Duration:** 4-8 weeks

---

## Phase 3: Input + Audio Backends
**Milestone:** Headless Swift binary loads ROM, runs 10s, reads gamepad via SDL2, streams audio.

- Define Swift protocols: `InputBackend`, `AudioBackend`
- Write `SDLInputBackend`: SDL2 gamepad API → `OESystemKey` button press/release model
  - Replaces: `OEDeviceManager` (from OpenEmuKit), `OEHIDEvent`
  - Handles controller hotplug via SDL2 event loop
- Write `SDLAudioBackend`: SDL2 audio ring buffer
  - Replaces: `OpenEmu/OEAudioDeviceManager.h/.m` (CoreAudio)
  - Device enumeration: `SDL_GetNumAudioDevices` / `SDL_GetAudioDeviceName`
- Write `LinuxPowerManager`: D-Bus `org.freedesktop.ScreenSaver` inhibit/uninhibit
  - Replaces: `IOPMAssertionID` usage in `OEGameDocument.swift`
- Replace `Carbon.HIToolbox.Events` keyboard handling with SDL2 keyboard events

**Complexity:** Medium | **Duration:** 3-5 weeks

---

## Phase 4: GTK4 Game Window (First Visible Output)
**Milestone:** `openemu-linux path/to/rom.sfc` opens GTK4 window showing SNES game at 60fps
with keyboard input and audio.

- Set up GTK4 Swift bindings (Adwaita-swift or direct C interop for missing widgets)
- `GtkApplication` entry point replacing `NSApplicationMain` / `AppDelegate`
- `GtkWindow` → `GtkGLArea` for the game rendering surface
  - Replaces: `OpenEmu/GameViewController.swift`
  - Replaces: `OEGameLayerView`, `OEScaledGameLayerView` (Metal/CALayer)
- OpenGL rendering: frame buffer → texture upload → textured quad
- Game loop: dedicated Swift `Thread` at 60Hz via `clock_nanosleep` (decoupled from GTK frame clock)
  - GTK4 render callback only uploads latest completed frame
- Wire keyboard events → SDL2 input backend → core button presses
- Implement passthrough GLSL shader, then port one CRT shader as proof-of-concept
- No library browser yet — just `open file dialog → game plays`

**Complexity:** High | **Duration:** 4-8 weeks

---

## Phase 5: Library Browser UI
**Milestone:** Full library browser with game grid, sidebar, box art, and game launch works.

- GTK4 window hierarchy:
  - `GtkApplicationWindow` → `GtkPaned` (sidebar | content) → `GtkStack` (grid/list)
- Sidebar: `GtkListBox` listing systems + collections
  - Replaces: `SidebarController.swift`
- Game grid: `GtkFlowBox` + custom cell widget with async box art loading
  - Replaces: `OEGridView.m` (871 lines, subclasses `IKImageBrowserView` — Apple private API)
  - Budget 3-4 weeks for this component alone
- List view: `GtkColumnView`
  - Replaces: `OEGameTableView`
- ROM import: drag-and-drop + menu → `ROMImporter` logic from Phase 2
- Box art: `OpenVGDB.swift` uses `Foundation.URLSession` — works on Linux, keep as-is
- Redesign `OEGameDocument.swift` (1,987 lines, `NSDocument` subclass) as a plain Swift class
  - This is the **highest-risk item** in the entire port
  - Do NOT incrementally refactor — rewrite from scratch against Phase 1-3 interfaces
  - Owns: core lifecycle, save states, screenshots, audio/video sync, pause/resume

**Critical file:** `OpenEmu/OEGameDocument.swift`

**Complexity:** Very High | **Duration:** 8-16 weeks

---

## Phase 6: Preferences, Shaders, Polish
**Milestone:** Complete, packageable application (AppImage or Flatpak).

- Preferences window (GTK4 dialog with stack pages):
  - Library (`PrefLibraryController.swift`)
  - Gameplay / shader selection (`PrefGameplayController.swift`)
  - Controller mapping (`PrefControlsController.swift`) — most complex pane
  - Cores (`PrefCoresController.swift`)
  - BIOS files (`PrefBiosController.swift`)
- Controller mapping UI: `GtkGrid` with controller image PNG + button labels
  - Replaces: `ControllerImageView.swift` + xib layouts
- Shader parameter UI: replaces `ShaderParametersWindowController.swift`
- Save state UI: replaces `SaveStateViewController.swift`
- In-game HUD: `GtkOverlay` widget — replaces `GameControlsBar.swift`
- Replace Sparkle with GitHub releases JSON version check
- Replace `XADMaster.framework` with `libarchive` bindings or system CLI
- `NSLocalizedString` compatibility shim for Swift on Linux bundle lookup

**Complexity:** Very High (breadth) | **Duration:** 8-12 weeks

---

## Phase 7 (Optional): Multi-Process Core Isolation
**Milestone:** Core crashes don't kill the UI; automatic recovery.

- Replace `NSXPCConnection` / XPC broker with Unix domain sockets
- `fork()` + Unix socket protocol for core helper process
- Shared memory for video frame buffers (avoid copying frame data over socket)
- Replaces: `OpenEmu/broker/main.swift` (XPC broker)

**Complexity:** High | **Duration:** 4-8 weeks

---

## Total Estimate (Solo Developer)

| Phase | Duration |
|---|---|
| 0: Build system + cores | 2-4 weeks |
| 1: Core C/Swift interface | 3-6 weeks |
| 2: Database (GRDB) | 4-8 weeks |
| 3: Input + audio | 3-5 weeks |
| 4: GTK4 game window | 4-8 weeks |
| 5: Library browser | 8-16 weeks |
| 6: Preferences + polish | 8-12 weeks |
| **Total (Phases 0-6)** | **32-59 weeks** |

---

## Key References

- **RetroArch**: `libretro.h` for C core interface design (Phase 1); OpenGL video backend (Phase 4)
- **GNOME Games** (GitLab): GTK4 game library browser in Vala — study GtkFlowBox game grid
- **GRDB.swift**: https://github.com/groue/GRDB.swift — SQLite ORM, no macOS deps
- **Adwaita-swift**: GTK4 Swift bindings for Linux
- **SDL2**: Audio + input backends

## Verification Commands

- Phase 0: `cd linux && cmake -B build && cmake --build build && ./build/test_harness <rom>`
- Phase 2: `swift run oelib-test` → CRUD operations, no import errors
- Phase 3: `swift run oe-headless <rom.sfc>` → gamepad input + audio for 10s
- Phase 4: `openemu-linux <rom.sfc>` → GTK4 window, game plays at 60fps
- Phase 5: Full app launch → library populates, game launches from grid
- Phase 6: Full app → prefs, controller mapping, shader selection, save states work
