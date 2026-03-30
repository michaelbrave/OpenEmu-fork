#ifndef OE_LIBRARY_DATABASE_H
#define OE_LIBRARY_DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QDateTime>
#include <vector>

namespace OpenEmu {

struct DbSystem {
    int id;
    QString identifier;
    QString name;
};

struct DbGame {
    int id;
    int systemId;
    QString title;
    QDateTime lastPlayedAt;
};

struct DbRom {
    int id;
    int gameId;
    QString path;
    QString md5;
};

struct DbSaveState {
    int id;
    int gameId;
    QString name;
    QString filePath;
    QDateTime createdAt;
};

struct DbCollection {
    int id;
    QString name;
};

class LibraryDatabase {
public:
    LibraryDatabase(const QString& path);
    ~LibraryDatabase();

    bool open();
    void close();

    // Migration / Setup
    bool migrate();

    // Systems
    int addSystem(const QString& identifier, const QString& name);
    std::vector<DbSystem> getAllSystems();
    DbSystem getSystem(const QString& identifier);

    // Games
    int addGame(int systemId, const QString& title);
    std::vector<DbGame> getGamesForSystem(const QString& systemIdentifier);
    bool updateLastPlayed(int gameId);

    // ROMs
    int addRom(int gameId, const QString& path, const QString& md5 = "");
    std::vector<DbRom> getRomsForGame(int gameId);
    DbRom getRomByPath(const QString& path);
    DbRom getRomByMd5(const QString& md5);
    bool updateRomMd5(int romId, const QString& md5);

    // Save States
    int addSaveState(int gameId, const QString& name, const QString& filePath);
    std::vector<DbSaveState> getSaveStatesForGame(int gameId);
    bool deleteSaveState(int saveStateId);

    // Collections
    int addCollection(const QString& name);
    std::vector<DbCollection> getAllCollections();
    bool addGameToCollection(int collectionId, int gameId);
    std::vector<DbGame> getGamesInCollection(int collectionId);

    // Direct query access
    QSqlQuery execute(const QString& query);

private:
    QString m_path;
    QSqlDatabase m_db;
};

// ROM scanning utilities
QString computeMd5(const QString& filePath);
QString guessSystemIdentifier(const QString& romPath);

} // namespace OpenEmu

#endif // OE_LIBRARY_DATABASE_H
