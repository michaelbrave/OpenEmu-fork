# OELinuxPort Swift Scaffold

This package is the Phase 2 + Phase 3 starter for the Linux port plan:

- Phase 2: GRDB-based library schema and CRUD foundation.
- Phase 3: Input/audio backend protocols plus SDL-named backend stubs.

## Layout

- `Sources/OELinuxPort/Database`: GRDB models, migrations, and `GRDBLibraryDatabase`.
- `Sources/OELinuxPort/Backends`: `InputBackend`, `AudioBackend`, and Linux backend placeholders.
- `Sources/oe-headless`: tiny CLI proving database + backend wiring.

## Build

```bash
cd linux/swift-port
swift build
swift run oe-headless ../../romstest/Super\ Mario\ World\ \(USA\)/Super\ Mario\ World\ \(USA\).sfc
```

## Notes

- SDL2 integration is intentionally stubbed for now.
- Next pass should replace `inject(...)` and in-memory audio queue with real SDL polling/callbacks.
- Expand schema to all OpenEmu entities once CoreData-to-GRDB mapping is validated.
