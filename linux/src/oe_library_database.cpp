#include "oe_library_database.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>

namespace OpenEmu {

LibraryDatabase::LibraryDatabase(const QString& path)
    : m_path(path)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(m_path);
}

LibraryDatabase::~LibraryDatabase()
{
    close();
}

bool LibraryDatabase::open()
{
    if (m_db.isOpen()) return true;
    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    return true;
}

void LibraryDatabase::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool LibraryDatabase::migrate()
{
    if (!open()) return false;

    QSqlQuery query(m_db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS systems ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "identifier TEXT UNIQUE NOT NULL,"
                    "name TEXT NOT NULL)")) {
        qCritical() << "Failed to create systems table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS games ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "systemId INTEGER NOT NULL,"
                    "title TEXT NOT NULL,"
                    "lastPlayedAt TEXT,"
                    "FOREIGN KEY(systemId) REFERENCES systems(id))")) {
        qCritical() << "Failed to create games table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS roms ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "gameId INTEGER NOT NULL,"
                    "path TEXT UNIQUE NOT NULL,"
                    "md5 TEXT,"
                    "FOREIGN KEY(gameId) REFERENCES games(id))")) {
        qCritical() << "Failed to create roms table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS save_states ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "gameId INTEGER NOT NULL,"
                    "name TEXT NOT NULL,"
                    "filePath TEXT UNIQUE NOT NULL,"
                    "createdAt TEXT NOT NULL,"
                    "FOREIGN KEY(gameId) REFERENCES games(id))")) {
        qCritical() << "Failed to create save_states table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS collections ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name TEXT UNIQUE NOT NULL)")) {
        qCritical() << "Failed to create collections table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS collection_games ("
                    "collectionId INTEGER NOT NULL,"
                    "gameId INTEGER NOT NULL,"
                    "PRIMARY KEY(collectionId, gameId),"
                    "FOREIGN KEY(collectionId) REFERENCES collections(id),"
                    "FOREIGN KEY(gameId) REFERENCES games(id))")) {
        qCritical() << "Failed to create collection_games table:" << query.lastError().text();
        return false;
    }

    // Indexes for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_games_systemId ON games(systemId)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_roms_gameId ON roms(gameId)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_roms_md5 ON roms(md5)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_save_states_gameId ON save_states(gameId)");

    return true;
}

// --- Systems ---

int LibraryDatabase::addSystem(const QString& identifier, const QString& name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO systems (identifier, name) VALUES (?, ?)");
    query.addBindValue(identifier);
    query.addBindValue(name);
    if (!query.exec()) return -1;

    if (query.numRowsAffected() == 0) {
        query.prepare("SELECT id FROM systems WHERE identifier = ?");
        query.addBindValue(identifier);
        if (query.exec() && query.next()) return query.value(0).toInt();
        return -1;
    }

    return query.lastInsertId().toInt();
}

std::vector<DbSystem> LibraryDatabase::getAllSystems()
{
    std::vector<DbSystem> systems;
    QSqlQuery query("SELECT id, identifier, name FROM systems", m_db);
    while (query.next()) {
        systems.push_back({
            query.value(0).toInt(),
            query.value(1).toString(),
            query.value(2).toString()
        });
    }
    return systems;
}

DbSystem LibraryDatabase::getSystem(const QString& identifier)
{
    DbSystem sys{-1, "", ""};
    QSqlQuery query(m_db);
    query.prepare("SELECT id, identifier, name FROM systems WHERE identifier = ?");
    query.addBindValue(identifier);
    if (query.exec() && query.next()) {
        sys.id = query.value(0).toInt();
        sys.identifier = query.value(1).toString();
        sys.name = query.value(2).toString();
    }
    return sys;
}

// --- Games ---

int LibraryDatabase::addGame(int systemId, const QString& title)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO games (systemId, title) VALUES (?, ?)");
    query.addBindValue(systemId);
    query.addBindValue(title);
    if (!query.exec()) return -1;
    return query.lastInsertId().toInt();
}

std::vector<DbGame> LibraryDatabase::getGamesForSystem(const QString& systemIdentifier)
{
    std::vector<DbGame> games;
    QSqlQuery query(m_db);
    query.prepare("SELECT games.id, games.systemId, games.title, games.lastPlayedAt "
                  "FROM games "
                  "JOIN systems ON games.systemId = systems.id "
                  "WHERE systems.identifier = ? "
                  "ORDER BY games.title COLLATE NOCASE");
    query.addBindValue(systemIdentifier);
    if (query.exec()) {
        while (query.next()) {
            games.push_back({
                query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toString(),
                QDateTime::fromString(query.value(3).toString(), Qt::ISODate)
            });
        }
    }
    return games;
}

bool LibraryDatabase::updateLastPlayed(int gameId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET lastPlayedAt = ? WHERE id = ?");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(gameId);
    return query.exec() && query.numRowsAffected() > 0;
}

// --- ROMs ---

int LibraryDatabase::addRom(int gameId, const QString& path, const QString& md5)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO roms (gameId, path, md5) VALUES (?, ?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(path);
    query.addBindValue(md5.isEmpty() ? QVariant() : md5);
    if (!query.exec()) return -1;
    return query.lastInsertId().toInt();
}

std::vector<DbRom> LibraryDatabase::getRomsForGame(int gameId)
{
    std::vector<DbRom> roms;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, gameId, path, md5 FROM roms WHERE gameId = ?");
    query.addBindValue(gameId);
    if (query.exec()) {
        while (query.next()) {
            roms.push_back({
                query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toString(),
                query.value(3).toString()
            });
        }
    }
    return roms;
}

DbRom LibraryDatabase::getRomByPath(const QString& path)
{
    DbRom rom{-1, -1, "", ""};
    QSqlQuery query(m_db);
    query.prepare("SELECT id, gameId, path, md5 FROM roms WHERE path = ?");
    query.addBindValue(path);
    if (query.exec() && query.next()) {
        rom.id = query.value(0).toInt();
        rom.gameId = query.value(1).toInt();
        rom.path = query.value(2).toString();
        rom.md5 = query.value(3).toString();
    }
    return rom;
}

DbRom LibraryDatabase::getRomByMd5(const QString& md5)
{
    DbRom rom{-1, -1, "", ""};
    QSqlQuery query(m_db);
    query.prepare("SELECT id, gameId, path, md5 FROM roms WHERE md5 = ?");
    query.addBindValue(md5);
    if (query.exec() && query.next()) {
        rom.id = query.value(0).toInt();
        rom.gameId = query.value(1).toInt();
        rom.path = query.value(2).toString();
        rom.md5 = query.value(3).toString();
    }
    return rom;
}

bool LibraryDatabase::updateRomMd5(int romId, const QString& md5)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE roms SET md5 = ? WHERE id = ?");
    query.addBindValue(md5);
    query.addBindValue(romId);
    return query.exec() && query.numRowsAffected() > 0;
}

// --- Save States ---

int LibraryDatabase::addSaveState(int gameId, const QString& name, const QString& filePath)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO save_states (gameId, name, filePath, createdAt) VALUES (?, ?, ?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(name);
    query.addBindValue(filePath);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec()) return -1;
    return query.lastInsertId().toInt();
}

std::vector<DbSaveState> LibraryDatabase::getSaveStatesForGame(int gameId)
{
    std::vector<DbSaveState> states;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, gameId, name, filePath, createdAt FROM save_states "
                  "WHERE gameId = ? ORDER BY createdAt DESC");
    query.addBindValue(gameId);
    if (query.exec()) {
        while (query.next()) {
            states.push_back({
                query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toString(),
                query.value(3).toString(),
                QDateTime::fromString(query.value(4).toString(), Qt::ISODate)
            });
        }
    }
    return states;
}

bool LibraryDatabase::deleteSaveState(int saveStateId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM save_states WHERE id = ?");
    query.addBindValue(saveStateId);
    return query.exec() && query.numRowsAffected() > 0;
}

// --- Collections ---

int LibraryDatabase::addCollection(const QString& name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO collections (name) VALUES (?)");
    query.addBindValue(name);
    if (!query.exec()) return -1;

    query.prepare("SELECT id FROM collections WHERE name = ?");
    query.addBindValue(name);
    if (query.exec() && query.next()) return query.value(0).toInt();
    return -1;
}

std::vector<DbCollection> LibraryDatabase::getAllCollections()
{
    std::vector<DbCollection> cols;
    QSqlQuery query("SELECT id, name FROM collections", m_db);
    while (query.next()) {
        cols.push_back({
            query.value(0).toInt(),
            query.value(1).toString()
        });
    }
    return cols;
}

bool LibraryDatabase::addGameToCollection(int collectionId, int gameId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO collection_games (collectionId, gameId) VALUES (?, ?)");
    query.addBindValue(collectionId);
    query.addBindValue(gameId);
    return query.exec();
}

std::vector<DbGame> LibraryDatabase::getGamesInCollection(int collectionId)
{
    std::vector<DbGame> games;
    QSqlQuery query(m_db);
    query.prepare("SELECT games.id, games.systemId, games.title, games.lastPlayedAt "
                  "FROM games "
                  "JOIN collection_games ON games.id = collection_games.gameId "
                  "WHERE collection_games.collectionId = ? "
                  "ORDER BY games.title COLLATE NOCASE");
    query.addBindValue(collectionId);
    if (query.exec()) {
        while (query.next()) {
            games.push_back({
                query.value(0).toInt(),
                query.value(1).toInt(),
                query.value(2).toString(),
                QDateTime::fromString(query.value(3).toString(), Qt::ISODate)
            });
        }
    }
    return games;
}

// --- ROM scanning utilities ---

QString computeMd5(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

QString guessSystemIdentifier(const QString& romPath)
{
    QString ext = QFileInfo(romPath).suffix().toLower();

    if (ext == "sfc" || ext == "smc") return "org.openemu.snes";
    if (ext == "gba") return "org.openemu.gba";
    if (ext == "nes") return "org.openemu.fceultra";
    if (ext == "gb" || ext == "gbc") return "org.openemu.gameboycolor";
    if (ext == "gen" || ext == "md" || ext == "bin") return "org.openemu.genesis";
    if (ext == "nds") return "org.openemu.desmume";
    if (ext == "n64" || ext == "z64") return "org.openemu.mupen64plus";
    if (ext == "psx" || ext == "cue" || ext == "pbp") return "org.openemu.mednafen";

    return "";
}

QSqlQuery LibraryDatabase::execute(const QString& query)
{
    return QSqlQuery(query, m_db);
}

} // namespace OpenEmu
