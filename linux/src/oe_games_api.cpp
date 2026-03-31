#include "oe_games_api.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <QRegularExpression>
#include <limits>

namespace OpenEmu {

static const QString TGDB_BASE_URL = QStringLiteral("https://api.thegamesdb.net/v1");

namespace {

QString normalizeTitleForSearch(QString title)
{
    title = QFileInfo(title).completeBaseName();
    title.replace('_', ' ');
    title.replace('.', ' ');

    static const QRegularExpression trailingTags(
        QStringLiteral("\\s*\\((?:[^()]*)\\)\\s*$"));
    while (trailingTags.match(title).hasMatch()) {
        title.remove(trailingTags);
    }

    title.replace(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]")), QStringLiteral(" "));
    title.replace(QRegularExpression(QStringLiteral("\\b(v\\d+(?:\\.\\d+)*)\\b"),
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral(" "));
    title.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return title.trimmed();
}

QStringList regionHintsForTitle(const QString& title)
{
    const QString lower = title.toLower();
    QStringList hints;
    if (lower.contains(QStringLiteral("usa")) || lower.contains(QStringLiteral("(us)"))) {
        hints << QStringLiteral("usa");
    }
    if (lower.contains(QStringLiteral("europe")) || lower.contains(QStringLiteral("(eu)"))) {
        hints << QStringLiteral("europe");
    }
    if (lower.contains(QStringLiteral("japan")) || lower.contains(QStringLiteral("(jp)"))) {
        hints << QStringLiteral("japan");
    }
    if (lower.contains(QStringLiteral("world"))) {
        hints << QStringLiteral("world");
    }
    return hints;
}

int scoreSearchResult(const QJsonObject& game, const QString& normalizedTitle, const QStringList& regionHints,
                      const QString& tgdbPlatform)
{
    int score = 0;
    const QString candidateTitle = game.value(QStringLiteral("game_title")).toString().trimmed();
    const QString normalizedCandidate = normalizeTitleForSearch(candidateTitle).toLower();
    const QString normalizedQuery = normalizedTitle.toLower();

    if (!tgdbPlatform.isEmpty() &&
        QString::number(game.value(QStringLiteral("platform")).toInt()) == tgdbPlatform) {
        score += 1000;
    }

    if (normalizedCandidate == normalizedQuery) {
        score += 500;
    } else if (normalizedCandidate.startsWith(normalizedQuery)) {
        score += 150;
    } else if (normalizedCandidate.contains(normalizedQuery)) {
        score += 50;
    }

    const int regionId = game.value(QStringLiteral("region_id")).toInt();
    const int countryId = game.value(QStringLiteral("country_id")).toInt();
    if (regionHints.contains(QStringLiteral("usa")) && (regionId == 1 || countryId == 50)) score += 75;
    if (regionHints.contains(QStringLiteral("europe")) && regionId == 2) score += 75;
    if (regionHints.contains(QStringLiteral("japan")) && (regionId == 4 || countryId == 28)) score += 75;
    if (regionHints.contains(QStringLiteral("world")) && regionId == 9) score += 75;

    return score;
}

QString extractBoxArtUrl(const QJsonObject& root, int gameId)
{
    const QJsonObject includeObj = root.value(QStringLiteral("include")).toObject();
    const QJsonObject boxartObj = includeObj.value(QStringLiteral("boxart")).toObject();
    const QJsonObject baseUrlObj = boxartObj.value(QStringLiteral("base_url")).toObject();
    const QString baseUrl = baseUrlObj.value(QStringLiteral("medium")).toString(
        baseUrlObj.value(QStringLiteral("original")).toString());
    const QJsonObject boxartData = boxartObj.value(QStringLiteral("data")).toObject();
    const QJsonArray artEntries = boxartData.value(QString::number(gameId)).toArray();

    QString fallback;
    for (const QJsonValue& value : artEntries) {
        const QJsonObject art = value.toObject();
        const QString filename = art.value(QStringLiteral("filename")).toString();
        if (filename.isEmpty()) continue;
        if (art.value(QStringLiteral("side")).toString() == QStringLiteral("front")) {
            return baseUrl + filename;
        }
        if (fallback.isEmpty()) {
            fallback = baseUrl + filename;
        }
    }
    return fallback;
}

QString joinIdMappedNames(const QJsonObject& root, const QString& mapName, const QJsonArray& ids)
{
    QStringList names;
    const QJsonObject includeObj = root.value(QStringLiteral("include")).toObject();
    const QJsonObject map = includeObj.value(mapName).toObject().value(QStringLiteral("data")).toObject();
    for (const QJsonValue& idValue : ids) {
        const QString id = QString::number(idValue.toInt());
        const QString name = map.value(id).toString();
        if (!name.isEmpty()) {
            names << name;
        }
    }
    names.removeDuplicates();
    return names.join(QStringLiteral(", "));
}

bool hasMeaningfulMetadata(const GameInfo& info)
{
    return !info.releaseDate.isEmpty() ||
           !info.developer.isEmpty() ||
           !info.publisher.isEmpty() ||
           !info.genre.isEmpty() ||
           !info.description.isEmpty() ||
           !info.boxArtUrl.isEmpty();
}

} // namespace

static const QMap<QString, PlatformMapping> createPlatformMappings() {
    QMap<QString, PlatformMapping> mappings;
    
    mappings[QStringLiteral("org.openemu.fceultra")] = {"7", QStringLiteral("fceultra")};
    mappings[QStringLiteral("org.openemu.nes")] = {"7", QStringLiteral("nes")};
    mappings[QStringLiteral("org.openemu.snes")] = {"6", QStringLiteral("snes")};
    mappings[QStringLiteral("org.openemu.gba")] = {"5", QStringLiteral("gba")};
    mappings[QStringLiteral("org.openemu.gameboy")] = {"4", QStringLiteral("gameboy")};
    mappings[QStringLiteral("org.openemu.gameboycolor")] = {"41", QStringLiteral("gameboycolor")};
    mappings[QStringLiteral("org.openemu.genesis")] = {"18", QStringLiteral("genesis")};
    mappings[QStringLiteral("org.openemu.mupen64plus")] = {"3", QStringLiteral("n64")};
    mappings[QStringLiteral("org.openemu.desmume")] = {"16", QStringLiteral("desmume")};
    
    return mappings;
}

const QMap<QString, PlatformMapping>& GamesDatabaseApi::platformMappings() {
    static const QMap<QString, PlatformMapping> mappings = createPlatformMappings();
    return mappings;
}

QString GamesDatabaseApi::systemIdToTgdbPlatform(const QString& systemId) {
    auto mappings = platformMappings();
    auto it = mappings.find(systemId);
    if (it != mappings.end()) {
        return it->tgdbId;
    }
    return QString();
}

GamesDatabaseApi::GamesDatabaseApi(QObject* parent)
    : QObject(parent)
{
    m_cacheDir = QDir::home().filePath(QStringLiteral(".local/share/openemu/gamesdb"));
    QDir().mkpath(m_cacheDir);
}

GamesDatabaseApi::~GamesDatabaseApi() = default;

void GamesDatabaseApi::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void GamesDatabaseApi::setScreenScraperCredentials(const QString& user, const QString& pass)
{
    m_ssUser = user;
    m_ssPass = pass;
}

void GamesDatabaseApi::setCacheDirectory(const QString& path)
{
    m_cacheDir = path;
    QDir().mkpath(m_cacheDir);
}

bool GamesDatabaseApi::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

QString GamesDatabaseApi::cacheKey(const QString& title, const QString& systemId)
{
    QString combined = systemId + QStringLiteral(":") + title;
    return QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5).toHex();
}

QString GamesDatabaseApi::imageCachePath(const QString& key)
{
    return m_cacheDir + QStringLiteral("/images/") + key + QStringLiteral(".png");
}

void GamesDatabaseApi::saveToCache(const QString& key, const GameInfo& info)
{
    QString cacheFile = m_cacheDir + QStringLiteral("/") + key + QStringLiteral(".json");
    QFile file(cacheFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = info.id;
        obj[QStringLiteral("tgdbId")] = info.tgdbId;
        obj[QStringLiteral("title")] = info.title;
        obj[QStringLiteral("platform")] = info.platform;
        obj[QStringLiteral("platformId")] = info.platformId;
        obj[QStringLiteral("boxArtUrl")] = info.boxArtUrl;
        obj[QStringLiteral("releaseDate")] = info.releaseDate;
        obj[QStringLiteral("developer")] = info.developer;
        obj[QStringLiteral("publisher")] = info.publisher;
        obj[QStringLiteral("genre")] = info.genre;
        obj[QStringLiteral("description")] = info.description;
        QJsonDocument doc(obj);
        file.write(doc.toJson());
    }
}

GameInfo GamesDatabaseApi::loadFromCache(const QString& key)
{
    GameInfo info;
    QString cacheFile = m_cacheDir + QStringLiteral("/") + key + QStringLiteral(".json");
    QFile file(cacheFile);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        info.id = obj[QStringLiteral("id")].toInt();
        info.tgdbId = obj[QStringLiteral("tgdbId")].toInt(info.id);
        info.title = obj[QStringLiteral("title")].toString();
        info.platform = obj[QStringLiteral("platform")].toString();
        info.platformId = obj[QStringLiteral("platformId")].toString();
        info.boxArtUrl = obj[QStringLiteral("boxArtUrl")].toString();
        info.releaseDate = obj[QStringLiteral("releaseDate")].toString();
        info.developer = obj[QStringLiteral("developer")].toString();
        info.publisher = obj[QStringLiteral("publisher")].toString();
        info.genre = obj[QStringLiteral("genre")].toString();
        info.description = obj[QStringLiteral("description")].toString();
        
        QString imagePath = imageCachePath(key);
        if (QFile::exists(imagePath)) {
            info.boxArt.load(imagePath);
        }
    }
    return info;
}

void GamesDatabaseApi::searchGame(const QString& title, const QString& systemId,
                                 std::function<void(const GameInfo&)> callback)
{
    const QString normalizedTitle = normalizeTitleForSearch(title);
    QString key = cacheKey(normalizedTitle.isEmpty() ? title : normalizedTitle, systemId);
    
    if (m_searchCache.contains(key)) {
        const GameInfo cached = m_searchCache[key];
        if (cached.id <= 0 || hasMeaningfulMetadata(cached)) {
            callback(cached);
            return;
        }
    }
    
    GameInfo cachedInfo = loadFromCache(key);
    if (cachedInfo.id > 0 && hasMeaningfulMetadata(cachedInfo)) {
        m_searchCache[key] = cachedInfo;
        callback(cachedInfo);
        return;
    }
    
    if (!hasApiKey()) {
        callback(GameInfo{});
        return;
    }
    
    QString tgdbPlatform = systemIdToTgdbPlatform(systemId);
    
    static QNetworkAccessManager* networkManager = nullptr;
    if (!networkManager) {
        networkManager = new QNetworkAccessManager(this);
    }
    
    QString encodedTitle = QUrl::toPercentEncoding(normalizedTitle.isEmpty() ? title : normalizedTitle);
    QString urlStr = QStringLiteral("%1/Games/ByGameName?name=%2&apikey=%3&include=boxart")
        .arg(TGDB_BASE_URL, encodedTitle, m_apiKey);
    
    if (!tgdbPlatform.isEmpty()) {
        urlStr += QStringLiteral("&platform=%1").arg(tgdbPlatform);
    }
    
    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
                     QStringLiteral("OpenEmu-Linux/1.0"));
    
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        GameInfo result{};
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            
            if (root.contains(QStringLiteral("data"))) {
                QJsonObject dataObj = root[QStringLiteral("data")].toObject();
                if (dataObj.contains(QStringLiteral("games"))) {
                    QJsonArray games = dataObj[QStringLiteral("games")].toArray();
                    if (!games.isEmpty()) {
                        const QStringList hints = regionHintsForTitle(title);
                        int bestScore = std::numeric_limits<int>::min();
                        QJsonObject game;
                        for (const QJsonValue& value : games) {
                            const QJsonObject candidate = value.toObject();
                            const int score = scoreSearchResult(candidate, normalizedTitle, hints, tgdbPlatform);
                            if (score > bestScore) {
                                bestScore = score;
                                game = candidate;
                            }
                        }

                        result.id = game[QStringLiteral("id")].toInt();
                        result.tgdbId = result.id;
                        result.title = game[QStringLiteral("game_title")].toString();
                        result.releaseDate = game[QStringLiteral("release_date")].toString();
                        
                        if (game.contains(QStringLiteral("platform"))) {
                            result.platformId = QString::number(game[QStringLiteral("platform")].toInt());
                        }
                        result.boxArtUrl = extractBoxArtUrl(root, result.id);

                        QString detailsUrl = QStringLiteral(
                            "%1/Games/ByGameID?apikey=%2&id=%3&fields=players,publishers,genres,overview,platform&include=boxart")
                            .arg(TGDB_BASE_URL, m_apiKey)
                            .arg(result.id);
                        QNetworkRequest detailsRequest{QUrl(detailsUrl)};
                        detailsRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                                 QStringLiteral("OpenEmu-Linux/1.0"));
                        QNetworkReply* detailsReply = networkManager->get(detailsRequest);
                        connect(detailsReply, &QNetworkReply::finished, this, [=]() mutable {
                            detailsReply->deleteLater();

                            if (detailsReply->error() == QNetworkReply::NoError) {
                                const QJsonDocument detailsDoc = QJsonDocument::fromJson(detailsReply->readAll());
                                const QJsonObject detailsRoot = detailsDoc.object();
                                const QJsonArray detailGames = detailsRoot.value(QStringLiteral("data"))
                                                                   .toObject()
                                                                   .value(QStringLiteral("games"))
                                                                   .toArray();
                                if (!detailGames.isEmpty()) {
                                    const QJsonObject detailGame = detailGames.first().toObject();
                                    result.releaseDate = detailGame.value(QStringLiteral("release_date")).toString(result.releaseDate);
                                    result.description = detailGame.value(QStringLiteral("overview")).toString();
                                    result.developer = joinIdMappedNames(detailsRoot, QStringLiteral("developers"),
                                                                         detailGame.value(QStringLiteral("developers")).toArray());
                                    result.publisher = joinIdMappedNames(detailsRoot, QStringLiteral("publishers"),
                                                                         detailGame.value(QStringLiteral("publishers")).toArray());
                                    result.genre = joinIdMappedNames(detailsRoot, QStringLiteral("genres"),
                                                                     detailGame.value(QStringLiteral("genres")).toArray());
                                    if (result.boxArtUrl.isEmpty()) {
                                        result.boxArtUrl = extractBoxArtUrl(detailsRoot, result.id);
                                    }
                                }
                            } else {
                                qWarning() << "TGDB details API error:" << detailsReply->errorString();
                            }

                            saveToCache(key, result);
                            m_searchCache[key] = result;
                            callback(result);
                        });
                        return;
                    }
                }
            }
        } else {
            qWarning() << "TGDB API error:" << reply->errorString();
        }
        
        callback(result);
    });
}

void GamesDatabaseApi::fetchBoxArt(const QString& url, const QString& gameTitle,
                                   std::function<void(const QPixmap&)> callback)
{
    if (url.isEmpty()) {
        callback(QPixmap{});
        return;
    }
    
    QString key = QString::fromUtf8(QCryptographicHash::hash(
        url.toUtf8(), QCryptographicHash::Md5).toHex());
    
    if (m_imageCache.contains(key)) {
        callback(m_imageCache[key]);
        return;
    }
    
    QString cachedPath = imageCachePath(key);
    if (QFile::exists(cachedPath)) {
        QPixmap pixmap(cachedPath);
        if (!pixmap.isNull()) {
            m_imageCache[key] = pixmap;
            callback(pixmap);
            return;
        }
    }
    
    static QNetworkAccessManager* networkManager = nullptr;
    if (!networkManager) {
        networkManager = new QNetworkAccessManager(this);
    }
    
    QUrl netUrl(url);
    QNetworkRequest request(netUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                     QStringLiteral("OpenEmu-Linux/1.0"));
    request.setRawHeader("Accept", "image/*");
    
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        QPixmap pixmap;
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            pixmap.loadFromData(data);
            
            if (!pixmap.isNull()) {
                QDir().mkpath(m_cacheDir + QStringLiteral("/images"));
                pixmap.save(cachedPath);
                m_imageCache[key] = pixmap;
            }
        } else {
            qWarning() << "Image fetch error:" << reply->errorString();
        }
        
        callback(pixmap);
    });
}

} // namespace OpenEmu
