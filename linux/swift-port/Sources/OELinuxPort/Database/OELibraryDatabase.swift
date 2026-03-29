import Foundation
import GRDB

public protocol LibraryDatabase {
    func migrate() throws
    func addSystem(identifier: String, name: String) throws -> OEDBSystem
    func addGame(systemId: Int64, title: String) throws -> OEDBGame
    func addROM(gameId: Int64, path: String, md5: String?) throws -> OEDBRom
    func addSaveState(gameId: Int64, name: String, filePath: String) throws -> OEDBSaveState
    func games(forSystemIdentifier identifier: String) throws -> [OEDBGame]
}

public final class GRDBLibraryDatabase: LibraryDatabase {
    private let dbQueue: DatabaseQueue

    public init(path: String) throws {
        dbQueue = try DatabaseQueue(path: path)
    }

    public func migrate() throws {
        var migrator = DatabaseMigrator()

        migrator.registerMigration("create_core_tables") { db in
            try db.create(table: OEDBSystem.databaseTableName) { t in
                t.autoIncrementedPrimaryKey("id")
                t.column("identifier", .text).notNull().unique()
                t.column("name", .text).notNull()
            }

            try db.create(table: OEDBGame.databaseTableName) { t in
                t.autoIncrementedPrimaryKey("id")
                t.column("systemId", .integer).notNull().indexed().references(OEDBSystem.databaseTableName, onDelete: .cascade)
                t.column("title", .text).notNull()
                t.column("lastPlayedAt", .datetime)
            }

            try db.create(table: OEDBRom.databaseTableName) { t in
                t.autoIncrementedPrimaryKey("id")
                t.column("gameId", .integer).notNull().indexed().references(OEDBGame.databaseTableName, onDelete: .cascade)
                t.column("path", .text).notNull().unique()
                t.column("md5", .text)
            }

            try db.create(table: OEDBSaveState.databaseTableName) { t in
                t.autoIncrementedPrimaryKey("id")
                t.column("gameId", .integer).notNull().indexed().references(OEDBGame.databaseTableName, onDelete: .cascade)
                t.column("name", .text).notNull()
                t.column("filePath", .text).notNull().unique()
                t.column("createdAt", .datetime).notNull()
            }
        }

        try migrator.migrate(dbQueue)
    }

    public func addSystem(identifier: String, name: String) throws -> OEDBSystem {
        try dbQueue.write { db in
            var system = OEDBSystem(identifier: identifier, name: name)
            try system.insert(db)
            return system
        }
    }

    public func addGame(systemId: Int64, title: String) throws -> OEDBGame {
        try dbQueue.write { db in
            var game = OEDBGame(systemId: systemId, title: title)
            try game.insert(db)
            return game
        }
    }

    public func addROM(gameId: Int64, path: String, md5: String?) throws -> OEDBRom {
        try dbQueue.write { db in
            var rom = OEDBRom(gameId: gameId, path: path, md5: md5)
            try rom.insert(db)
            return rom
        }
    }

    public func addSaveState(gameId: Int64, name: String, filePath: String) throws -> OEDBSaveState {
        try dbQueue.write { db in
            var saveState = OEDBSaveState(gameId: gameId, name: name, filePath: filePath)
            try saveState.insert(db)
            return saveState
        }
    }

    public func games(forSystemIdentifier identifier: String) throws -> [OEDBGame] {
        let sql = """
        SELECT games.*
        FROM games
        INNER JOIN systems ON systems.id = games.systemId
        WHERE systems.identifier = ?
        ORDER BY games.title COLLATE NOCASE
        """
        return try dbQueue.read { db in
            try OEDBGame.fetchAll(db, sql: sql, arguments: [identifier])
        }
    }
}
