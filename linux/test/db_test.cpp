#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <iostream>
#include "oe_library_database.h"

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        qCritical() << "FAIL:" << msg; \
        g_failures++; \
    } else { \
        qDebug() << "PASS:" << msg; \
    } \
} while(0)

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QString dbPath = "oelib_test.sqlite";
    QFile::remove(dbPath);

    OpenEmu::LibraryDatabase db(dbPath);

    // --- Migration ---
    CHECK(db.migrate(), "migration succeeds");

    // --- Systems ---
    int snesId = db.addSystem("org.openemu.snes", "Super Nintendo");
    CHECK(snesId > 0, "addSystem SNES");

    int gbaId = db.addSystem("org.openemu.gba", "Game Boy Advance");
    CHECK(gbaId > 0, "addSystem GBA");

    int snesId2 = db.addSystem("org.openemu.snes", "SNES Duplicate");
    CHECK(snesId2 == snesId, "addSystem duplicate returns existing id");

    auto systems = db.getAllSystems();
    CHECK(systems.size() == 2, "getAllSystems returns 2");

    auto sys = db.getSystem("org.openemu.snes");
    CHECK(sys.id == snesId, "getSystem by identifier");

    // --- Games ---
    int smwId = db.addGame(snesId, "Super Mario World");
    CHECK(smwId > 0, "addGame SMW");

    int metroidId = db.addGame(snesId, "Super Metroid");
    CHECK(metroidId > 0, "addGame Super Metroid");

    int emeraldId = db.addGame(gbaId, "Pokemon Emerald");
    CHECK(emeraldId > 0, "addGame Pokemon Emerald");

    auto snesGames = db.getGamesForSystem("org.openemu.snes");
    CHECK(snesGames.size() == 2, "getGamesForSystem SNES returns 2");

    CHECK(db.updateLastPlayed(smwId), "updateLastPlayed");

    // --- ROMs ---
    int rom1 = db.addRom(smwId, "/roms/smw.sfc", "d1a2b3c4e5f6a7b8");
    CHECK(rom1 > 0, "addRom SMW");

    int rom2 = db.addRom(emeraldId, "/roms/emerald.gba");
    CHECK(rom2 > 0, "addRom Emerald (no md5)");

    CHECK(db.updateRomMd5(rom2, "deadbeef12345678"), "updateRomMd5");

    auto roms = db.getRomsForGame(smwId);
    CHECK(roms.size() == 1, "getRomsForGame SMW");
    CHECK(!roms.empty() && roms[0].md5 == "d1a2b3c4e5f6a7b8", "ROM md5 matches");

    auto byPath = db.getRomByPath("/roms/smw.sfc");
    CHECK(byPath.id == rom1, "getRomByPath finds ROM");

    auto byMd5 = db.getRomByMd5("deadbeef12345678");
    CHECK(byMd5.id == rom2, "getRomByMd5 finds ROM");

    // --- Save States ---
    int ss1 = db.addSaveState(smwId, "Slot 1", "/saves/smw_slot1.save");
    CHECK(ss1 > 0, "addSaveState");

    int ss2 = db.addSaveState(smwId, "Slot 2", "/saves/smw_slot2.save");
    CHECK(ss2 > 0, "addSaveState Slot 2");

    auto states = db.getSaveStatesForGame(smwId);
    CHECK(states.size() == 2, "getSaveStatesForGame returns 2");

    CHECK(db.deleteSaveState(ss2), "deleteSaveState");
    states = db.getSaveStatesForGame(smwId);
    CHECK(states.size() == 1, "getSaveStatesForGame returns 1 after delete");

    // --- Collections ---
    int col1 = db.addCollection("Favorites");
    CHECK(col1 > 0, "addCollection Favorites");

    int col2 = db.addCollection("RPGs");
    CHECK(col2 > 0, "addCollection RPGs");

    int col1dup = db.addCollection("Favorites");
    CHECK(col1dup == col1, "addCollection duplicate returns existing");

    auto cols = db.getAllCollections();
    CHECK(cols.size() == 2, "getAllCollections returns 2");

    CHECK(db.addGameToCollection(col1, smwId), "addGameToCollection");
    CHECK(db.addGameToCollection(col1, emeraldId), "addGameToCollection");

    auto colGames = db.getGamesInCollection(col1);
    CHECK(colGames.size() == 2, "getGamesInCollection returns 2");

    // --- MD5 utility ---
    // Create a temp file to hash
    QString tmpRom = "/tmp/oe_test_rom.bin";
    {
        QFile f(tmpRom);
        f.open(QIODevice::WriteOnly);
        f.write("test rom data for hashing");
    }
    QString hash = OpenEmu::computeMd5(tmpRom);
    CHECK(!hash.isEmpty(), "computeMd5 returns non-empty");
    CHECK(hash.length() == 32, "computeMd5 returns 32-char hex");

    // --- System guesser ---
    QString sysGuess = OpenEmu::guessSystemIdentifier("/path/to/game.sfc");
    CHECK(sysGuess == "org.openemu.snes", "guessSystemIdentifier .sfc");

    sysGuess = OpenEmu::guessSystemIdentifier("/path/to/game.gba");
    CHECK(sysGuess == "org.openemu.gba", "guessSystemIdentifier .gba");

    sysGuess = OpenEmu::guessSystemIdentifier("/path/to/game.nes");
    CHECK(sysGuess == "org.openemu.fceultra", "guessSystemIdentifier .nes");

    sysGuess = OpenEmu::guessSystemIdentifier("/path/to/game.xyz");
    CHECK(sysGuess.isEmpty(), "guessSystemIdentifier unknown ext");

    // --- Summary ---
    qDebug() << "----------------------------------------";
    if (g_failures == 0) {
        qDebug() << "ALL TESTS PASSED.";
    } else {
        qCritical() << g_failures << "TEST(S) FAILED.";
    }

    return g_failures > 0 ? 1 : 0;
}
