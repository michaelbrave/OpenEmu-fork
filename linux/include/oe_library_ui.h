#ifndef OE_LIBRARY_UI_H
#define OE_LIBRARY_UI_H

#include <QMainWindow>
#include <QSplitter>
#include <QListView>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QToolBar>
#include <QAction>
#include <QMenuBar>
#include <QStatusBar>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QMenu>
#include <functional>
#include "oe_library_database.h"
#include "oe_emulator_view.h"
#include "oe_game_loop.h"
#include "oe_cover_art.h"
#include "oe_cover_art_dialog.h"
#include "oe_games_api.h"
#include "oe_edit_game_info_dialog.h"

namespace OpenEmu {

class Sidebar : public QListView {
public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar();

    void loadSystems();
    QString currentSystemId() const;
    void selectSystemId(const QString& systemId);
    void setLibraryDatabase(LibraryDatabase* db);
    
    std::function<void(const QString&)> onSystemSelected;

private:
    LibraryDatabase* m_db;
    QStandardItemModel* m_model;
};

class GameGridView : public QWidget {
public:
    explicit GameGridView(QWidget* parent = nullptr);
    ~GameGridView();

    void setLibraryDatabase(LibraryDatabase* db);
    void loadGamesForSystem(const QString& systemId);
    void filterGames(const QString& searchText);
    void setApiKey(const QString& key);
    GamesDatabaseApi* gamesApi() const { return m_gamesApi; }
    CoverArtDownloader* coverDownloader() const { return m_coverDownloader; }

    std::function<void(int64_t)> onGameSelected;
    std::function<void(const QStringList&)> onFilesDropped;
    std::function<void(int64_t)> onEditGameInfo;
    std::function<void(int64_t)> onDownloadCoverArt;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    LibraryDatabase* m_db;
    QListView* m_listView;
    QStandardItemModel* m_model;
    QString m_currentSystemId;
    QString m_searchFilter;
    std::map<int64_t, QStandardItem*> m_gameItems;
    GamesDatabaseApi* m_gamesApi;
    CoverArtDownloader* m_coverDownloader;
    int64_t m_contextMenuGameId;

    void setupUI();
    void showContextMenu(const QPoint& pos);
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void setLibraryDatabase(LibraryDatabase* db);
    void loadROM(const QString& path);

    std::function<void(const QString&, const QString&)> onRomSelected;

public slots:
    void onSearch(const QString& text);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    LibraryDatabase* m_db;
    QWidget* m_headerWidget;
    QLineEdit* m_searchBox;
    QSplitter* m_splitter;
    Sidebar* m_sidebar;
    GameGridView* m_gameGrid;
    QStackedWidget* m_contentStack;
    
    QAction* m_importAction;
    QAction* m_preferencesAction;
    
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupHeader();
    void connectSignals();

private slots:
    void onImportROMs();
    void showPreferences();
    void onPreferencesAccepted();

private:
    void importROM(const QString& path);
    void applyStyleSheet();
};

} // namespace OpenEmu

#endif // OE_LIBRARY_UI_H
