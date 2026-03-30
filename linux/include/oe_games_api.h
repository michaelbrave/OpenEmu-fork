#ifndef OE_GAMES_API_H
#define OE_GAMES_API_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QPixmap>
#include <functional>

namespace OpenEmu {

struct GameInfo {
    int id;
    QString title;
    QString platform;
    QString platformId;
    QString boxArtUrl;
    QPixmap boxArt;
};

struct PlatformMapping {
    QString tgdbId;
    QString systemId;
};

class GamesDatabaseApi : public QObject {
public:
    explicit GamesDatabaseApi(QObject* parent = nullptr);
    ~GamesDatabaseApi();

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }

    void setCacheDirectory(const QString& path);
    QString cacheDirectory() const { return m_cacheDir; }

    bool isConfigured() const;
    bool hasApiKey() const { return !m_apiKey.isEmpty(); }

    void searchGame(const QString& title, const QString& systemId,
                    std::function<void(const GameInfo&)> callback);
    
    void fetchBoxArt(const QString& url, const QString& gameTitle,
                    std::function<void(const QPixmap&)> callback);

    static QString systemIdToTgdbPlatform(const QString& systemId);

    static const QMap<QString, PlatformMapping>& platformMappings();

private:
    QString m_apiKey;
    QString m_cacheDir;
    QMap<QString, QPixmap> m_imageCache;
    QMap<QString, GameInfo> m_searchCache;

    QString cacheKey(const QString& title, const QString& systemId);
    QString imageCachePath(const QString& key);
    void saveToCache(const QString& key, const GameInfo& info);
    GameInfo loadFromCache(const QString& key);
};

} // namespace OpenEmu

#endif // OE_GAMES_API_H
