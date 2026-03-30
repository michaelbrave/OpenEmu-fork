#include "oe_library_ui.h"
#include "oe_preferences_dialog.h"
#include "oe_edit_game_info_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
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
/* Custom model role to mark sidebar section header rows */
static const int kSidebarSectionRole = Qt::UserRole + 2;

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
    const int SIZE = TILE_ICON_SIZE;
    const int RADIUS = 10;

    QPixmap pixmap(SIZE, SIZE);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    /* Clip to rounded rect */
    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(0, 0, SIZE, SIZE), RADIUS, RADIUS);
    p.setClipPath(clipPath);

    /* Main gradient */
    QLinearGradient bg(0, 0, 0, SIZE);
    bg.setColorAt(0.0, style.primary);
    bg.setColorAt(1.0, style.secondary);
    p.fillRect(0, 0, SIZE, SIZE, bg);

    /* Top sheen highlight */
    QLinearGradient sheen(0, 0, 0, SIZE * 0.45);
    sheen.setColorAt(0.0, QColor(255, 255, 255, 55));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(0, 0, SIZE, (int)(SIZE * 0.45), sheen);

    /* Game name */
    p.setPen(Qt::white);
    QFont nameFont("Sans", 10, QFont::Bold);
    p.setFont(nameFont);
    QString displayName = name.section('.', 0, 0);
    p.drawText(QRect(10, 10, SIZE - 20, SIZE - 20),
               Qt::AlignCenter | Qt::TextWordWrap, displayName);

    p.setClipping(false);

    /* Subtle border */
    p.setPen(QPen(QColor(255, 255, 255, 40), 1.0));
    p.drawRoundedRect(QRectF(0.5, 0.5, SIZE - 1, SIZE - 1), RADIUS, RADIUS);

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
    setObjectName("gameSidebar");
    setModel(m_model);
    setMinimumWidth(SIDEBAR_WIDTH);
    setMaximumWidth(SIDEBAR_WIDTH + 50);
    setUniformItemSizes(false);

    connect(this, &QListView::clicked, this, [this](const QModelIndex& idx) {
        if (!idx.isValid() || !onSystemSelected) return;
        /* Skip section header rows */
        if (idx.data(kSidebarSectionRole).toBool()) return;
        QString systemId = idx.data(Qt::UserRole).toString();
        if (!systemId.isEmpty()) onSystemSelected(systemId);
    });
}

Sidebar::~Sidebar() = default;

void Sidebar::setLibraryDatabase(LibraryDatabase* db)
{
    m_db = db;
    loadSystems();
}

static QStandardItem* makeSectionHeader(const QString& label)
{
    auto item = new QStandardItem(label);
    item->setFlags(Qt::ItemIsEnabled); /* visible but not selectable */
    item->setData(true, kSidebarSectionRole);
    QFont f = item->font();
    f.setPointSize(9);
    f.setWeight(QFont::Medium);
    item->setFont(f);
    item->setForeground(QColor("#888888"));
    return item;
}

void Sidebar::loadSystems()
{
    m_model->clear();

    m_model->appendRow(makeSectionHeader("LIBRARY"));

    auto allItem = new QStandardItem("All Games");
    allItem->setData("all", Qt::UserRole);
    m_model->appendRow(allItem);

    auto recentItem = new QStandardItem("Recent");
    recentItem->setData("recent", Qt::UserRole);
    m_model->appendRow(recentItem);

    if (!m_db) {
        setCurrentIndex(m_model->index(1, 0));
        return;
    }

    m_model->appendRow(makeSectionHeader("CONSOLES"));

    QSqlQuery query = m_db->execute("SELECT id, name FROM systems ORDER BY name");
    while (query.next()) {
        auto item = new QStandardItem(query.value("name").toString());
        item->setData(query.value("id").toString(), Qt::UserRole);
        m_model->appendRow(item);
    }

    /* Select "All Games" (row 1, not the section header at row 0) */
    setCurrentIndex(m_model->index(1, 0));
}

QString Sidebar::currentSystemId() const
{
    QModelIndex idx = currentIndex();
    if (!idx.isValid()) return QString();
    return idx.data(Qt::UserRole).toString();
}

void Sidebar::selectSystemId(const QString& systemId)
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex idx = m_model->index(row, 0);
        if (!idx.isValid()) continue;
        if (idx.data(kSidebarSectionRole).toBool()) continue;
        if (idx.data(Qt::UserRole).toString() == systemId) {
            setCurrentIndex(idx);
            if (onSystemSelected) onSystemSelected(systemId);
            return;
        }
    }
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

    m_listView->setObjectName("gameGrid");
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setIconSize(QSize(GRID_ICON_SIZE, GRID_ICON_SIZE));
    m_listView->setGridSize(QSize(GRID_ICON_SIZE + 24, GRID_ICON_SIZE + 52));
    m_listView->setSpacing(GRID_SPACING);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setModel(m_model);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setWordWrap(true);
    m_listView->setTextElideMode(Qt::ElideMiddle);

    layout->addWidget(m_listView);

    /* Single click: notify selection */
    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.isValid() && onGameSelected) {
            int64_t gameId = idx.data(Qt::UserRole).toLongLong();
            if (gameId > 0) onGameSelected(gameId);
        }
    });

    /* Double click: launch game */
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex& idx) {
        if (idx.isValid() && onGameSelected) {
            int64_t gameId = idx.data(Qt::UserRole).toLongLong();
            if (gameId > 0) onGameSelected(gameId);
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
        
        m_model->appendRow(item);
        m_gameItems[gameId] = item;
        
        m_coverDownloader->fetchCover(name, systemIdentifier, 
            [item, name, systemIdentifier](const QPixmap& cover) {
                if (!cover.isNull()) {
                    item->setIcon(QIcon(cover));
                }
            });
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
    
    QAction* downloadCoverAction = menu.addAction("Set Cover Art...");
    downloadCoverAction->setIcon(QIcon::fromTheme("image-x-generic"));
    
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
            "ROM files (*.nes *.snes *.sfc *.smc *.gb *.gbc *.gba *.n64 *.z64 *.v64 *.md *.gen *.bin *.nds *.cue *.pbp);;All files (*.*)");
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
    toolbar->setFixedHeight(38);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_importAction = toolbar->addAction(
        QIcon::fromTheme("list-add", QIcon::fromTheme("document-open")),
        "Import ROMs…");
    m_importAction->setToolTip("Import ROM files into the library");
    connect(m_importAction, &QAction::triggered, this, &MainWindow::onImportROMs);

    toolbar->addSeparator();

    m_preferencesAction = toolbar->addAction(
        QIcon::fromTheme("preferences-system"),
        "Preferences");
    m_preferencesAction->setToolTip("Open preferences");
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);
}

void MainWindow::setupHeader()
{
    m_headerWidget = new QWidget;
    m_headerWidget->setFixedHeight(56);
    m_headerWidget->setObjectName("libraryHeader");

    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel("OpenEmu");
    titleLabel->setObjectName("libraryTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);

    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText("Search...");
    m_searchBox->setFixedWidth(220);
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
            QString("SELECT g.title, s.name as systemName, s.identifier FROM games g "
                    "JOIN systems s ON s.id = g.systemId "
                    "WHERE g.id = %1").arg(gameId));
        
        if (query.next()) {
            QString gameName = query.value("title").toString();
            QString systemName = query.value("systemName").toString();
            QString systemId = query.value("identifier").toString();

            CoverArtDialog dialog(this);
            dialog.setGameInfo(gameName, systemName);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            if (m_gameGrid->coverDownloader()->saveCover(gameName, systemId, dialog.selectedCover())) {
                statusBar()->showMessage("Cover art updated: " + gameName);
                m_gameGrid->loadGamesForSystem(m_sidebar->currentSystemId());
            } else {
                statusBar()->showMessage("Failed to save cover art");
            }
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
        "ROM files (*.nes *.snes *.sfc *.smc *.gb *.gbc *.gba *.n64 *.z64 *.v64 *.md *.gen *.bin *.nds *.cue *.pbp);;All files (*.*)"
    );
    
    qDebug() << "Selected files:" << paths;
    
    if (!paths.isEmpty()) {
        for (const QString& path : paths) {
            importROM(path);
        }
        m_sidebar->selectSystemId("all");
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
        m_sidebar->selectSystemId("all");
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
        /* === Base === */
        QMainWindow, QWidget {
            background-color: #1c1c1e;
            color: #f0f0f0;
            font-size: 13px;
        }

        /* === Header === */
        QWidget#libraryHeader {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                         stop:0 #3a3a3c, stop:1 #2c2c2e);
            border-bottom: 1px solid #404040;
        }
        QLabel#libraryTitle {
            color: #f0f0f0;
            font-size: 16px;
            font-weight: 600;
        }

        /* === Search box === */
        QLineEdit#searchBox {
            background-color: #3a3a3c;
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 13px;
            padding: 4px 14px;
            font-size: 13px;
            selection-background-color: #0a84ff;
        }
        QLineEdit#searchBox:focus {
            border: 1px solid #0a84ff;
            background-color: #444446;
        }

        /* === Toolbar === */
        QToolBar {
            background-color: #2c2c2e;
            border-bottom: 1px solid #404040;
            spacing: 6px;
            padding: 3px 10px;
        }
        QToolButton {
            background-color: #3a3a3c;
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 5px;
            padding: 4px 12px;
            font-size: 13px;
        }
        QToolButton:hover {
            background-color: #4a4a4c;
            border-color: #777777;
        }
        QToolButton:pressed {
            background-color: #222224;
        }
        QToolBar::separator {
            background-color: #444444;
            width: 1px;
            margin: 4px 6px;
        }

        /* === Menu bar === */
        QMenuBar {
            background-color: #2c2c2e;
            color: #f0f0f0;
            border-bottom: 1px solid #404040;
        }
        QMenuBar::item {
            padding: 4px 10px;
        }
        QMenuBar::item:selected {
            background-color: #3a3a3c;
            border-radius: 4px;
        }
        QMenu {
            background-color: #2c2c2e;
            color: #f0f0f0;
            border: 1px solid #505050;
            border-radius: 6px;
        }
        QMenu::item {
            padding: 5px 20px;
        }
        QMenu::item:selected {
            background-color: #0a84ff;
            color: #ffffff;
            border-radius: 4px;
        }
        QMenu::separator {
            height: 1px;
            background: #404040;
            margin: 3px 8px;
        }

        /* === Status bar === */
        QStatusBar {
            background-color: #2c2c2e;
            color: #ababab;
            border-top: 1px solid #404040;
        }

        /* === Splitter === */
        QSplitter::handle {
            background-color: #383838;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }

        /* === Sidebar (Sidebar is itself a QListView) === */
        QListView#gameSidebar {
            background-color: #252527;
            color: #d0d0d0;
            border: none;
            outline: none;
            font-size: 13px;
        }
        QListView#gameSidebar::item {
            border-radius: 5px;
            padding: 5px 8px;
            margin: 1px 6px;
        }
        QListView#gameSidebar::item:selected {
            background-color: #0a84ff;
            color: #ffffff;
        }
        QListView#gameSidebar::item:hover:!selected {
            background-color: #333335;
        }

        /* === Game grid === */
        QListView#gameGrid {
            background-color: #1c1c1e;
            color: #f0f0f0;
            border: none;
            outline: none;
        }
        QListView#gameGrid::item {
            border-radius: 8px;
            padding: 6px;
            margin: 4px;
            color: #d8d8d8;
            font-size: 12px;
        }
        QListView#gameGrid::item:selected {
            background-color: rgba(10, 132, 255, 0.25);
            color: #ffffff;
        }
        QListView#gameGrid::item:hover:!selected {
            background-color: rgba(255, 255, 255, 0.06);
        }

        /* === Scroll bars === */
        QScrollBar:vertical {
            background-color: transparent;
            width: 8px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background-color: #555557;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #777779;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        QScrollBar:horizontal {
            background-color: transparent;
            height: 8px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background-color: #555557;
            border-radius: 4px;
            min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #777779;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )");
}

} // namespace OpenEmu
