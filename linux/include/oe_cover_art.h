#ifndef OE_COVER_ART_H
#define OE_COVER_ART_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QMap>
#include <functional>

namespace OpenEmu {

class GamesDatabaseApi;

class CoverArtDownloader : public QObject {
public:
    explicit CoverArtDownloader(QObject* parent = nullptr);
    ~CoverArtDownloader();

    void fetchCover(const QString& gameTitle, const QString& systemId,
                    std::function<void(const QPixmap&)> callback);
    
    QPixmap getCachedCover(const QString& gameTitle, const QString& systemId);
    
    void setCacheDirectory(const QString& path);
    
    void setGamesDatabaseApi(GamesDatabaseApi* api);
    void setApiKey(const QString& key);
    
private:
    QString m_cacheDir;
    QMap<QString, QPixmap> m_cache;
    GamesDatabaseApi* m_gamesApi;
    
    QString cacheKey(const QString& gameTitle, const QString& systemId);
    QString sanitizeFilename(const QString& name);
};

} // namespace OpenEmu

#endif // OE_COVER_ART_H
