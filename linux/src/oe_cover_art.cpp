#include "oe_cover_art.h"
#include "oe_games_api.h"
#include <QDir>
#include <QCryptographicHash>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QRegularExpression>
#include <QUrl>

namespace OpenEmu {

static const int TILE_SIZE = 160;

static QPixmap generatePlaceholderTile(const QString& name, const QString& systemId)
{
    QColor primary("#0f3460");
    QColor secondary("#16213e");
    QString systemLabel;
    
    if (systemId.contains("fceultra") || systemId.contains("nes")) {
        primary = QColor("#c41e3a");
        secondary = QColor("#8b0000");
        systemLabel = "NES";
    } else if (systemId.contains("snes")) {
        primary = QColor("#6b5b95");
        secondary = QColor("#4a3f6b");
        systemLabel = "SNES";
    } else if (systemId.contains("gba")) {
        primary = QColor("#88b04a");
        secondary = QColor("#556b2f");
        systemLabel = "GBA";
    } else if (systemId.contains("gameboy")) {
        primary = QColor("#6b8e23");
        secondary = QColor("#556b2f");
        systemLabel = "GB";
    } else if (systemId.contains("genesis") || systemId.contains("gen")) {
        primary = QColor("#dd4124");
        secondary = QColor("#8b2500");
        systemLabel = "GENESIS";
    } else if (systemId.contains("mupen") || systemId.contains("n64")) {
        primary = QColor("#009b77");
        secondary = QColor("#006644");
        systemLabel = "N64";
    }
    
    QPixmap pixmap(TILE_SIZE, TILE_SIZE);
    pixmap.fill(Qt::darkGray);
    QPainter p(&pixmap);
    
    QLinearGradient gradient(0, 0, 0, TILE_SIZE);
    gradient.setColorAt(0, primary);
    gradient.setColorAt(1, secondary);
    p.fillRect(pixmap.rect(), gradient);
    
    QRect paddingRect = pixmap.rect().adjusted(8, 8, -8, -8);
    
    if (!systemLabel.isEmpty()) {
        p.setPen(QColor(255, 255, 255, 60));
        p.setFont(QFont("Sans", 8, QFont::Bold));
        p.drawText(paddingRect.adjusted(0, 0, 0, -20), Qt::AlignTop | Qt::AlignLeft, systemLabel);
    }
    
    p.setPen(Qt::white);
    p.setFont(QFont("Sans", 10, QFont::Bold));
    QString displayName = name.section('.', 0, 0);
    if (displayName.length() > 20) {
        displayName = displayName.left(18) + "...";
    }
    p.drawText(paddingRect.adjusted(0, systemLabel.isEmpty() ? 0 : -16, 0, 0), 
               Qt::AlignCenter | Qt::TextWordWrap, displayName);
    
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.drawRect(paddingRect.adjusted(-2, -2, 2, 2));
    p.end();
    
    return pixmap;
}

CoverArtDownloader::CoverArtDownloader(QObject* parent)
    : QObject(parent)
    , m_gamesApi(nullptr)
{
    QString cachePath = QDir::home().filePath(".local/share/openemu/covers");
    setCacheDirectory(cachePath);
}

CoverArtDownloader::~CoverArtDownloader() = default;

void CoverArtDownloader::setCacheDirectory(const QString& path)
{
    m_cacheDir = path;
    QDir().mkpath(m_cacheDir);
}

QString CoverArtDownloader::cacheKey(const QString& gameTitle, const QString& systemId)
{
    QString combined = systemId + ":" + gameTitle;
    return QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5).toHex();
}

QString CoverArtDownloader::sanitizeFilename(const QString& name)
{
    QString result = name;
    result.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    return result.left(100);
}

QPixmap CoverArtDownloader::getCachedCover(const QString& gameTitle, const QString& systemId)
{
    QString key = cacheKey(gameTitle, systemId);
    
    if (m_cache.contains(key)) {
        return m_cache[key];
    }
    
    QString filename = m_cacheDir + "/" + key + ".png";
    if (QFile::exists(filename)) {
        QPixmap pixmap(filename);
        if (!pixmap.isNull()) {
            m_cache[key] = pixmap;
            return pixmap;
        }
    }
    
    return QPixmap();
}

void CoverArtDownloader::setGamesDatabaseApi(GamesDatabaseApi* api)
{
    m_gamesApi = api;
}

void CoverArtDownloader::setApiKey(const QString& key)
{
    if (m_gamesApi) {
        m_gamesApi->setApiKey(key);
    }
}

void CoverArtDownloader::fetchCover(const QString& gameTitle, const QString& systemId,
                                     std::function<void(const QPixmap&)> callback)
{
    QPixmap cached = getCachedCover(gameTitle, systemId);
    if (!cached.isNull()) {
        callback(cached);
        return;
    }
    
    if (m_gamesApi && m_gamesApi->hasApiKey()) {
        m_gamesApi->searchGame(gameTitle, systemId, [this, gameTitle, systemId, callback](const GameInfo& info) {
            if (info.id > 0 && !info.boxArtUrl.isEmpty()) {
                m_gamesApi->fetchBoxArt(info.boxArtUrl, gameTitle, [this, gameTitle, systemId, callback](const QPixmap& cover) {
                    if (!cover.isNull()) {
                        QString key = cacheKey(gameTitle, systemId);
                        QString filename = m_cacheDir + "/" + key + ".png";
                        cover.save(filename);
                        m_cache[key] = cover;
                        callback(cover);
                    } else {
                        QPixmap placeholder = generatePlaceholderTile(gameTitle, systemId);
                        callback(placeholder);
                    }
                });
            } else {
                QPixmap placeholder = generatePlaceholderTile(gameTitle, systemId);
                callback(placeholder);
            }
        });
        return;
    }
    
    QString sanitizedTitle = sanitizeFilename(gameTitle);
    QStringList searchDirs = {
        QDir::home().filePath(".local/share/openemu/covers/custom"),
        QDir::home().filePath(".local/share/openemu/covers/" + systemId),
        QDir::home().filePath(".config/OpenEmu/covers"),
    };
    
    QStringList extensions = {"png", "jpg", "jpeg", "webp"};
    for (const QString& dir : searchDirs) {
        QDir coverDir(dir);
        if (coverDir.exists()) {
            for (const QString& ext : extensions) {
                QString localPath = coverDir.filePath(sanitizedTitle + "." + ext);
                if (QFile::exists(localPath)) {
                    QPixmap cover(localPath);
                    if (!cover.isNull()) {
                        m_cache[cacheKey(gameTitle, systemId)] = cover;
                        callback(cover);
                        return;
                    }
                }
            }
        }
    }
    
    QPixmap placeholder = generatePlaceholderTile(gameTitle, systemId);
    callback(placeholder);
}

} // namespace OpenEmu
