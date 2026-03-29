import Foundation
import GRDB

public struct OEDBSystem: Codable, FetchableRecord, MutablePersistableRecord {
    public static let databaseTableName = "systems"

    public var id: Int64?
    public var identifier: String
    public var name: String

    public init(id: Int64? = nil, identifier: String, name: String) {
        self.id = id
        self.identifier = identifier
        self.name = name
    }
}

public struct OEDBGame: Codable, FetchableRecord, MutablePersistableRecord {
    public static let databaseTableName = "games"

    public var id: Int64?
    public var systemId: Int64
    public var title: String
    public var lastPlayedAt: Date?

    public init(id: Int64? = nil, systemId: Int64, title: String, lastPlayedAt: Date? = nil) {
        self.id = id
        self.systemId = systemId
        self.title = title
        self.lastPlayedAt = lastPlayedAt
    }
}

public struct OEDBRom: Codable, FetchableRecord, MutablePersistableRecord {
    public static let databaseTableName = "roms"

    public var id: Int64?
    public var gameId: Int64
    public var path: String
    public var md5: String?

    public init(id: Int64? = nil, gameId: Int64, path: String, md5: String? = nil) {
        self.id = id
        self.gameId = gameId
        self.path = path
        self.md5 = md5
    }
}

public struct OEDBSaveState: Codable, FetchableRecord, MutablePersistableRecord {
    public static let databaseTableName = "save_states"

    public var id: Int64?
    public var gameId: Int64
    public var name: String
    public var filePath: String
    public var createdAt: Date

    public init(id: Int64? = nil, gameId: Int64, name: String, filePath: String, createdAt: Date = Date()) {
        self.id = id
        self.gameId = gameId
        self.name = name
        self.filePath = filePath
        self.createdAt = createdAt
    }
}
