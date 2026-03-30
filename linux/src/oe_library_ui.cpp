#include "oe_library_ui.h"
#include "oe_preferences_dialog.h"
#include "oe_edit_game_info_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QLineEdit>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QDialog>
#include <QProcess>
#include <map>

namespace OpenEmu {

static const int TILE_ICON_SIZE = 160;

struct SystemStyle {
    QColor primary;
    QColor secondary;
};

static const std::map<QString, SystemStyle> SYSTEM_STYLES = {
    {"org.openemu.fceultra", {QColor("#c41e3a"), QColor("#8b0000")}},
    {"org.openemu.snes", {QColor("#6b5b95"), QColor("#4a3f6b")}},
    {"org.openemu.gba", {QColor("#88b04a"), QColor("#556b2f")}},
    {"org.openemu.gameboycolor", {QColor("#88b04a"), QColor("#556b2f")}},
    {"org.openemu.genesis", {QColor("#dd4124"), QColor("#8b2500")}},
    {"org.openemu.mupen64plus", {QColor("#009b77"), QColor("#006644")}},
    {"org.openemu.desmume", {QColor("#45b8ac"), QColor("#2f7a72")}},
    {"org.openemu.mednafen", {QColor("#5b5ea6"), QColor("#3d3d7a")}},
};

static SystemStyle getSystemStyle(const QString& systemId) {
    auto it = SYSTEM_STYLES.find(systemId);
    if (it != SYSTEM_STYLES.end()) return it->second;
    return {QColor("#0f3460"), QColor("#16213e")};
}

static QPixmap generateGameTile(const QString& name, const QString& systemId) {
    SystemStyle style = getSystemStyle(systemId);
    
    QPixmap pixmap(TILE_ICON_SIZE, TILE_ICON_SIZE);
    pixmap.fill(Qt::darkGray);
    QPainter p(&pixmap);
    
    QLinearGradient gradient(0, 0, 0, TILE_ICON_SIZE);
    gradient.setColorAt(0, style.primary);
    gradient.setColorAt(1, style.secondary);
    p.fillRect(pixmap.rect(), gradient);
    
    QRect paddingRect = pixmap.rect().adjusted(12, 12, -12, -12);
    p.setPen(Qt::white);
    p.setFont(QFont("Sans", 10, QFont::Bold));
    p.drawText(paddingRect, Qt::AlignCenter | Qt::TextWordWrap, name.section('.', 0, 0));
    
    p.setPen(QPen(Qt::white, 1));
    p.drawRect(paddingRect.adjusted(-2, -2, 2, 2));
    
    p.end();
    return pixmap;
}

static const int SIDEBAR_WIDTH = 220;
static const int GRID_ICON_SIZE = 160;
static const int GRID_SPACING = 16;

Sidebar::Sidebar(QWidget* parent)
    : QListView(parent)
    , m_db(nullptr)
    , m_model(new QStandardItemModel(this))
{
    setModel(m_model);
    setMinimumWidth(SIDEBAR_WIDTH);
    setMaximumWidth(SIDEBAR_WIDTH + 50);
    setUniformItemSizes(true);
    
    connect(this, &QListView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.isValid() && onSystemSelected) {
            QString systemId = idx.data(Qt::UserRole).toString();
            onSystemSelected(systemId);
        }
    });
}

Sidebar::~Sidebar() = default;

void Sidebar::setLibraryDatabase(LibraryDatabase* db)
{
    m_db = db;
    loadSystems();
}

void Sidebar::loadSystems()
{
    m_model->clear();
    
    auto allItem = new QStandardItem("All Games");
    allItem->setData("all", Qt::UserRole);
    m_model->appendRow(allItem);
    
    auto recentItem = new QStandardItem("Recent");
    recentItem->setData("recent", Qt::UserRole);
    m_model->appendRow(recentItem);
    
    m_model->appendRow(new QStandardItem());
    
    if (!m_db) return;
    
    QSqlQuery query = m_db->execute("SELECT id, name FROM systems ORDER BY name");
    while (query.next()) {
        auto item = new QStandardItem(query.value("name").toString());
        item->setData(query.value("id").toString(), Qt::UserRole);
        m_model->appendRow(item);
    }
    
    setCurrentIndex(m_model->index(0, 0));
}

QString Sidebar::currentSystemId() const
{
    QModelIndex idx = currentIndex();
    if (!idx.isValid()) return QString();
    return idx.data(Qt::UserRole).toString();
}

GameGridView::GameGridView(QWidget* parent)
    : QWidget(parent)
    , m_db(nullptr)
    , m_listView(new QListView(this))
    , m_model(new QStandardItemModel(this))
    , m_gamesApi(new GamesDatabaseApi(this))
    , m_coverDownloader(new CoverArtDownloader(this))
    , m_contextMenuGameId(0)
{
    m_coverDownloader->setGamesDatabaseApi(m_gamesApi);
    
    QSettings settings("OpenEmu", "Linux");
    QString apiKey = settings.value("tgdbApiKey").toString();
    if (!apiKey.isEmpty()) {
        m_coverDownloader->setApiKey(apiKey);
    }
    
    setAcceptDrops(true);
    setupUI();
    
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, &GameGridView::showContextMenu);
}

GameGridView::~GameGridView() = default;

void GameGridView::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setGridSize(QSize(GRID_ICON_SIZE + 20, GRID_ICON_SIZE + 60));
    m_listView->setSpacing(GRID_SPACING);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setModel(m_model);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    
    layout->addWidget(m_listView);
    
    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.isValid() && onGameSelected) {
            int64_t gameId = idx.data(Qt::UserRole).toLongLong();
            onGameSelected(gameId);
        }
    });
}

void GameGridView::setLibraryDatabase(LibraryDatabase* db)
{
    m_db = db;
}

void GameGridView::loadGamesForSystem(const QString& systemId)
{
    m_model->clear();
    m_gameItems.clear();
    m_currentSystemId = systemId;
    
    if (!m_db) return;
    
    QString queryStr;
    if (systemId.isEmpty() || systemId == "all") {
        queryStr = "SELECT g.id, g.title, MIN(r.path) AS path, s.name as system_name, s.identifier "
                   "FROM games g "
                   "JOIN systems s ON g.systemId = s.id "
                   "JOIN roms r ON r.gameId = g.id "
                   "GROUP BY g.id, g.title, s.name, s.identifier "
                   "ORDER BY g.title";
    } else if (systemId == "recent") {
        queryStr = "SELECT g.id, g.title, MIN(r.path) AS path, s.name as system_name, s.identifier "
                   "FROM games g "
                   "JOIN systems s ON g.systemId = s.id "
                   "JOIN roms r ON r.gameId = g.id "
                   "GROUP BY g.id, g.title, s.name, s.identifier, g.lastPlayedAt "
                   "ORDER BY g.lastPlayedAt DESC LIMIT 50";
    } else {
        queryStr = QString("SELECT g.id, g.title, MIN(r.path) AS path, s.name as system_name, s.identifier "
                          "FROM games g "
                          "JOIN systems s ON g.systemId = s.id "
                          "JOIN roms r ON r.gameId = g.id "
                          "WHERE s.id = %1 "
                          "GROUP BY g.id, g.title, s.name, s.identifier "
                          "ORDER BY g.title").arg(systemId);
    }
    
    QSqlQuery query = m_db->execute(queryStr);
    
    int row = 0, col = 0;
    const int cols = 6;
    
    while (query.next()) {
        int64_t gameId = query.value("id").toLongLong();
        QString name = query.value("title").toString();
        QString romPath = query.value("path").toString();
        QString systemIdentifier = query.value("identifier").toString();
        
        auto item = new QStandardItem();
        item->setData(QVariant::fromValue(gameId), Qt::UserRole);
        item->setText(name);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        item->setToolTip(QString("%1\n%2").arg(name, romPath));
        
        QPixmap placeholder = generateGameTile(name, systemIdentifier);
        item->setIcon(QIcon(placeholder));
        
        m_model->setItem(row, col, item);
        m_gameItems[gameId] = item;
        
        m_coverDownloader->fetchCover(name, systemIdentifier, 
            [item, name, systemIdentifier](const QPixmap& cover) {
                if (!cover.isNull()) {
                    item->setIcon(QIcon(cover));
                }
            });
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    
    if (!m_searchFilter.isEmpty()) {
        filterGames(m_searchFilter);
    }
}

void GameGridView::filterGames(const QString& searchText)
{
    m_searchFilter = searchText;
    
    for (auto& pair : m_gameItems) {
        QStandardItem* item = pair.second;
        if (item) {
            QString title = item->text();
            bool matches = title.contains(searchText, Qt::CaseInsensitive);
            item->setEnabled(matches);
            item->setSelectable(matches);
        }
    }
}

void GameGridView::setApiKey(const QString& key)
{
    if (m_coverDownloader) {
        m_coverDownloader->setApiKey(key);
    }
}

void GameGridView::showContextMenu(const QPoint& pos)
{
    QModelIndex idx = m_listView->indexAt(pos);
    if (!idx.isValid()) return;
    
    int64_t gameId = idx.data(Qt::UserRole).toLongLong();
    if (gameId == 0) return;
    
    m_contextMenuGameId = gameId;
    
    QMenu menu;
    
    QAction* playAction = menu.addAction("Play");
    playAction->setIcon(QIcon::fromTheme("media-playback-start"));
    
    menu.addSeparator();
    
    QAction* editInfoAction = menu.addAction("Edit Info...");
    editInfoAction->setIcon(QIcon::fromTheme("document-edit"));
    
    QAction* downloadCoverAction = menu.addAction("Download Cover Art");
    downloadCoverAction->setIcon(QIcon::fromTheme("download"));
    
    menu.addSeparator();
    
    QAction* showInFolderAction = menu.addAction("Show in File Manager");
    showInFolderAction->setIcon(QIcon::fromTheme("folder"));
    
    menu.addSeparator();
    
    QAction* deleteAction = menu.addAction("Delete from Library");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    deleteAction->setEnabled(false);
    
    QAction* selected = menu.exec(m_listView->mapToGlobal(pos));
    
    if (!selected) return;
    
    if (selected == playAction) {
        if (onGameSelected) {
            onGameSelected(gameId);
        }
    } else if (selected == editInfoAction) {
        if (onEditGameInfo) {
            onEditGameInfo(gameId);
        }
    } else if (selected == downloadCoverAction) {
        if (onDownloadCoverArt) {
            onDownloadCoverArt(gameId);
        }
    } else if (selected == showInFolderAction) {
        if (m_db) {
            QSqlQuery query = m_db->execute(
                QString("SELECT r.path FROM roms r WHERE r.gameId = %1").arg(gameId));
            if (query.next()) {
                QString romPath = query.value("path").toString();
                if (!romPath.isEmpty()) {
                    QProcess::startDetached("xdg-open",
                        {QFileInfo(romPath).absolutePath()});
                }
            }
        }
    }
}

void GameGridView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void GameGridView::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime->hasUrls()) return;
    
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (info.isFile()) {
            paths << path;
        }
    }
    
    if (!paths.isEmpty() && onFilesDropped) {
        onFilesDropped(paths);
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_headerWidget(nullptr)
    , m_searchBox(nullptr)
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_sidebar(new Sidebar(this))
    , m_gameGrid(new GameGridView(this))
    , m_contentStack(new QStackedWidget(this))
    , m_importAction(nullptr)
    , m_preferencesAction(nullptr)
{
    setAcceptDrops(true);
    applyStyleSheet();
    setupHeader();
    setupUI();
    setupMenuBar();
    setupToolBar();
    connectSignals();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    mainLayout->addWidget(m_headerWidget);
    
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_gameGrid);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_splitter);
    
    m_contentStack->addWidget(centralWidget);
    
    setCentralWidget(m_contentStack);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu("File");
    
    QAction* openRomAction = fileMenu->addAction("Open ROM...");
    openRomAction->setShortcut(QKeySequence::Open);
    connect(openRomAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Open ROM", QDir::homePath(),
            "ROM files (*.nes *.snes *.sfc *.gb *.gbc *.gba *.n64 *.z64 *.md *.gen);;All files (*.*)");
        if (!path.isEmpty() && onRomSelected) {
            onRomSelected(path, QString());
        }
    });
    
    QAction* importAction = fileMenu->addAction("Import ROMs to Library...");
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportROMs);
    
    fileMenu->addSeparator();
    
    QAction* quitAction = fileMenu->addAction("Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    QMenu* editMenu = menuBar()->addMenu("Edit");
    QAction* prefsAction = editMenu->addAction("Preferences");
    prefsAction->setShortcut(QKeySequence::Preferences);
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);
    
    QMenu* viewMenu = menuBar()->addMenu("View");
    QAction* toggleSidebarAction = viewMenu->addAction("Toggle Sidebar");
    connect(toggleSidebarAction, &QAction::triggered, this, [this]() {
        QList<int> sizes = m_splitter->sizes();
        const bool sidebarVisible = !sizes.isEmpty() && sizes[0] > 0;
        if (sidebarVisible) {
            m_splitter->setSizes({0, width()});
        } else {
            m_splitter->setSizes({SIDEBAR_WIDTH, qMax(1, width() - SIDEBAR_WIDTH)});
        }
    });
    
    QMenu* helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About OpenEmu");
}

void MainWindow::setupToolBar()
{
    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setFixedHeight(36);
    
    m_importAction = toolbar->addAction("Import");
    connect(m_importAction, &QAction::triggered, this, &MainWindow::onImportROMs);
    
    m_preferencesAction = toolbar->addAction("Preferences");
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);
}

void MainWindow::setupHeader()
{
    m_headerWidget = new QWidget;
    m_headerWidget->setFixedHeight(60);
    m_headerWidget->setObjectName("libraryHeader");
    
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    
    QLabel* titleLabel = new QLabel("OpenEmu");
    titleLabel->setObjectName("libraryTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    
    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText("Search games...");
    m_searchBox->setMinimumWidth(250);
    m_searchBox->setObjectName("searchBox");
    m_searchBox->setClearButtonEnabled(true);
    
    connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearch);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_searchBox);
}

void MainWindow::onSearch(const QString& text)
{
    if (m_gameGrid) {
        m_gameGrid->filterGames(text);
    }
}

void MainWindow::connectSignals()
{
    m_sidebar->onSystemSelected = [this](const QString& systemId) {
        m_gameGrid->loadGamesForSystem(systemId);
    };
    
    m_gameGrid->onGameSelected = [this](int64_t gameId) {
        if (!m_db) return;
        
        QSqlQuery query = m_db->execute(
            QString("SELECT r.path, s.identifier FROM games g "
                    "JOIN roms r ON r.gameId = g.id "
                    "JOIN systems s ON s.id = g.systemId "
                    "WHERE g.id = %1 "
                    "ORDER BY r.id LIMIT 1").arg(gameId));
        
        if (query.next()) {
            QString romPath = query.value("path").toString();
            QString systemId = query.value("identifier").toString();
            if (!romPath.isEmpty() && onRomSelected) {
                onRomSelected(romPath, systemId);
            } else {
                QMessageBox::warning(this, "Missing ROM",
                    "This library entry does not have an associated ROM file.");
            }
        } else {
            QMessageBox::warning(this, "Missing ROM",
                "This library entry no longer has a playable ROM record.");
        }
    };
    
    m_gameGrid->onFilesDropped = [this](const QStringList& paths) {
        for (const QString& path : paths) {
            importROM(path);
        }
        m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
    };
    
    m_gameGrid->onEditGameInfo = [this](int64_t gameId) {
        if (!m_db) return;
        
        QSqlQuery query = m_db->execute(
            QString("SELECT g.title, s.name as systemName, s.identifier, r.path FROM games g "
                    "JOIN systems s ON s.id = g.systemId "
                    "JOIN roms r ON r.gameId = g.id "
                    "WHERE g.id = %1").arg(gameId));
        
        if (query.next()) {
            QString title = query.value("title").toString();
            QString system = query.value("systemName").toString();
            QString systemId = query.value("identifier").toString();
            QString path = query.value("path").toString();
            
            EditGameInfoDialog dialog(this);
            dialog.setGameInfo(title, systemId, path);
            
            if (dialog.exec() == QDialog::Accepted) {
                QString newTitle = dialog.editedTitle();
                QString newSystemId = dialog.editedSystem();
                
                if (!newTitle.isEmpty() && newTitle != title) {
                    m_db->execute(QString("UPDATE games SET title = '%1' WHERE id = %2")
                        .arg(newTitle.replace("'", "''")).arg(gameId));
                }
                
                if (!newSystemId.isEmpty() && newSystemId != systemId) {
                    auto systems = m_db->getAllSystems();
                    int newSystemDbId = -1;
                    for (const auto& sys : systems) {
                        if (sys.identifier == newSystemId) {
                            newSystemDbId = sys.id;
                            break;
                        }
                    }
                    
                    if (newSystemDbId > 0) {
                        m_db->execute(QString("UPDATE games SET systemId = %1 WHERE id = %2")
                            .arg(newSystemDbId).arg(gameId));
                    }
                }
                
                m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
            }
        }
    };
    
    m_gameGrid->onDownloadCoverArt = [this](int64_t gameId) {
        if (!m_db) return;
        
        QSqlQuery query = m_db->execute(
            QString("SELECT g.title, s.identifier FROM games g "
                    "JOIN systems s ON s.id = g.systemId "
                    "WHERE g.id = %1").arg(gameId));
        
        if (query.next()) {
            QString gameName = query.value("title").toString();
            QString systemId = query.value("identifier").toString();
            
            QSettings settings("OpenEmu", "Linux");
            QString apiKey = settings.value("tgdbApiKey").toString();
            
            if (apiKey.isEmpty()) {
                QMessageBox::warning(this, "No API Key",
                    "Please set your TheGamesDB API key in Preferences.");
                return;
            }
            
            statusBar()->showMessage("Searching for cover art: " + gameName);
            
            m_gameGrid->gamesApi()->searchGame(gameName, systemId,
                [this, gameId, gameName](const GameInfo& info) {
                    if (info.boxArtUrl.isEmpty()) {
                        statusBar()->showMessage("No cover art found for: " + gameName);
                        return;
                    }
                    
                    m_gameGrid->gamesApi()->fetchBoxArt(info.boxArtUrl, gameName,
                        [this, gameId, info](const QPixmap& pixmap) {
                            if (!pixmap.isNull()) {
                                QString cachePath = QDir::home().filePath(
                                    ".local/share/openemu/covers");
                                QDir().mkpath(cachePath);
                                QString coverPath = cachePath + "/" +
                                    QString::number(gameId) + ".png";
                                pixmap.save(coverPath);
                                
                                statusBar()->showMessage("Cover art downloaded: " + info.title);
                                m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
                            } else {
                                statusBar()->showMessage("Failed to download cover art");
                            }
                        });
                });
        }
    };
}

void MainWindow::setLibraryDatabase(LibraryDatabase* db)
{
    m_db = db;
    m_sidebar->setLibraryDatabase(db);
    m_gameGrid->setLibraryDatabase(db);
    
    if (db) {
        m_gameGrid->loadGamesForSystem("all");
    }
}

void MainWindow::onImportROMs()
{
    qDebug() << "onImportROMs called";
    
    QStringList paths = QFileDialog::getOpenFileNames(
        this,
        "Import ROMs",
        QDir::homePath(),
        "ROM files (*.nes *.snes *.sfc *.gb *.gbc *.gba *.n64 *.z64 *.md *.gen);;All files (*.*)"
    );
    
    qDebug() << "Selected files:" << paths;
    
    if (!paths.isEmpty()) {
        for (const QString& path : paths) {
            importROM(path);
        }
        m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
    }
}

void MainWindow::loadROM(const QString& path)
{
    if (onRomSelected) {
        onRomSelected(path, QString());
    }
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        onPreferencesAccepted();
    }
}

void MainWindow::onPreferencesAccepted()
{
    QSettings settings("OpenEmu", "Linux");
    QString apiKey = settings.value("tgdbApiKey").toString();
    if (!apiKey.isEmpty() && m_gameGrid) {
        m_gameGrid->setApiKey(apiKey);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime->hasUrls()) return;
    
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (info.isFile()) {
            paths << path;
        }
    }
    
    if (!paths.isEmpty()) {
        for (const QString& path : paths) {
            importROM(path);
        }
        m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
    }
}

void MainWindow::importROM(const QString& path)
{
    if (!m_db) {
        qWarning() << "Import failed: No database";
        return;
    }
    
    QString systemId = guessSystemIdentifier(path);
    if (systemId.isEmpty()) {
        qWarning() << "Import failed: Unknown system for" << path;
        statusBar()->showMessage("Unknown ROM format: " + QFileInfo(path).fileName());
        return;
    }
    
    QString md5 = computeMd5(path);
    QString name = QFileInfo(path).fileName();

    DbRom existingRom = m_db->getRomByPath(path);
    if (existingRom.id > 0) {
        statusBar()->showMessage("Already in library: " + name);
        qDebug() << "Skipped duplicate ROM path:" << path;
        return;
    }
    
    auto systems = m_db->getAllSystems();
    int systemDbId = -1;
    for (const auto& sys : systems) {
        if (sys.identifier == systemId) {
            systemDbId = sys.id;
            break;
        }
    }
    
    if (systemDbId == -1) {
        systemDbId = m_db->addSystem(systemId, systemId.section('.', -1).toUpper());
        qDebug() << "Created new system:" << systemId;
    }
    
    int gameId = m_db->addGame(systemDbId, name);
    if (gameId > 0) {
        int romId = m_db->addRom(gameId, path, md5);
        if (romId > 0) {
            qDebug() << "Imported game:" << name << "ID:" << gameId;
            statusBar()->showMessage("Imported: " + name);
        } else {
            m_db->execute(QString("DELETE FROM games WHERE id = %1").arg(gameId));
            qWarning() << "Failed to add ROM to database:" << path;
            statusBar()->showMessage("Import failed: " + name);
        }
    } else {
        qWarning() << "Failed to add game to database:" << name;
        statusBar()->showMessage("Import failed: " + name);
    }
}

void MainWindow::applyStyleSheet()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1a1a2e;
        }
        QWidget#libraryHeader {
            background-color: #16213e;
            border-bottom: 1px solid #0f3460;
        }
        QLabel#libraryTitle {
            color: #e94560;
        }
        QLineEdit#searchBox {
            background-color: #0f3460;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 16px;
            padding: 6px 16px;
            font-size: 14px;
        }
        QLineEdit#searchBox:focus {
            border: 1px solid #e94560;
        }
        QLineEdit#searchBox::placeholder {
            color: #888;
        }
        QToolBar {
            background-color: #1a1a2e;
            border: none;
            padding: 4px 8px;
        }
        QToolBar QLabel {
            color: #eaeaea;
        }
        QToolButton {
            background-color: #0f3460;
            color: #eaeaea;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
        }
        QToolButton:hover {
            background-color: #e94560;
        }
        QToolBar::separator {
            background-color: #0f3460;
            width: 1px;
            margin: 4px 8px;
        }
        QMenuBar {
            background-color: #16213e;
            color: #eaeaea;
            border-bottom: 1px solid #0f3460;
        }
        QMenuBar::item:selected {
            background-color: #0f3460;
        }
        QMenu {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
        }
        QMenu::item:selected {
            background-color: #0f3460;
        }
        QStatusBar {
            background-color: #16213e;
            color: #aaa;
        }
        QListView {
            background-color: #1a1a2e;
            border: none;
            outline: none;
        }
        QListView::item {
            background-color: transparent;
            border-radius: 8px;
            padding: 4px;
        }
        QListView::item:selected {
            background-color: #e94560;
            border: 2px solid #ff6b8a;
        }
        QListView::item:hover:!selected {
            background-color: #2a2a4e;
            border: 1px solid #3a3a5e;
        }
        QScrollBar:vertical {
            background-color: #1a1a2e;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background-color: #0f3460;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #e94560;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )");
}

} // namespace OpenEmu
