#include "oe_games_api.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QDebug>

namespace OpenEmu {

static const QString TGDB_BASE_URL = QStringLiteral("https://api.thegamesdb.net/v1");

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
        obj[QStringLiteral("title")] = info.title;
        obj[QStringLiteral("platform")] = info.platform;
        obj[QStringLiteral("platformId")] = info.platformId;
        obj[QStringLiteral("boxArtUrl")] = info.boxArtUrl;
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
        info.title = obj[QStringLiteral("title")].toString();
        info.platform = obj[QStringLiteral("platform")].toString();
        info.platformId = obj[QStringLiteral("platformId")].toString();
        info.boxArtUrl = obj[QStringLiteral("boxArtUrl")].toString();
        
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
    QString key = cacheKey(title, systemId);
    
    if (m_searchCache.contains(key)) {
        callback(m_searchCache[key]);
        return;
    }
    
    GameInfo cachedInfo = loadFromCache(key);
    if (cachedInfo.id > 0) {
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
    
    QString encodedTitle = QUrl::toPercentEncoding(title);
    QString urlStr = QStringLiteral("%1/Games/ByGameName?name=%2&apikey=%3")
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
        
        GameInfo result;
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            
            if (root.contains(QStringLiteral("data"))) {
                QJsonObject dataObj = root[QStringLiteral("data")].toObject();
                if (dataObj.contains(QStringLiteral("games"))) {
                    QJsonArray games = dataObj[QStringLiteral("games")].toArray();
                    if (!games.isEmpty()) {
                        QJsonObject game = games[0].toObject();
                        result.id = game[QStringLiteral("id")].toInt();
                        result.title = game[QStringLiteral("game_title")].toString();
                        
                        if (game.contains(QStringLiteral("platform"))) {
                            result.platformId = QString::number(game[QStringLiteral("platform")].toInt());
                        }
                        
                        if (game.contains(QStringLiteral("boxart"))) {
                            QJsonObject boxart = game[QStringLiteral("boxart")].toObject();
                            QString baseUrl = boxart[QStringLiteral("base_url")].toString();
                            QString boxartPath = boxart[QStringLiteral("data")].toString();
                            if (!baseUrl.isEmpty() && !boxartPath.isEmpty()) {
                                result.boxArtUrl = baseUrl + boxartPath;
                            }
                        }
                        
                        saveToCache(key, result);
                        m_searchCache[key] = result;
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
