import Foundation
import OELinuxPort

@main
struct OELinuxHeadlessCLI {
    static func main() throws {
        let args = CommandLine.arguments
        guard args.count >= 2 else {
            fputs("usage: oe-headless <path-to-rom>\n", stderr)
            return
        }

        let dbPath = FileManager.default.currentDirectoryPath + "/oelib.sqlite"
        let db = try GRDBLibraryDatabase(path: dbPath)
        try db.migrate()

        let system = try db.addSystem(identifier: "snes", name: "Super Nintendo")
        let game = try db.addGame(systemId: system.id ?? 0, title: URL(fileURLWithPath: args[1]).deletingPathExtension().lastPathComponent)
        _ = try db.addROM(gameId: game.id ?? 0, path: args[1], md5: nil)

        let input = SDLInputBackend()
        try input.start()
        input.inject(.press("Start"))
        let events = input.pollEvents()
        input.stop()

        let audio = SDLAudioBackend()
        try audio.start(sampleRate: 48_000, channels: 2)
        let silence = Array(repeating: Int16(0), count: 512)
        silence.withUnsafeBufferPointer { audio.enqueue(samples: $0) }
        audio.stop()

        print("db: \(dbPath)")
        print("rom indexed: \(args[1])")
        print("input events captured: \(events.count)")
    }
}
