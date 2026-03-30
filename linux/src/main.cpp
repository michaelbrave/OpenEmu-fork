#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QVBoxLayout>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <getopt.h>
#include <SDL2/SDL.h>

#include "oe_library_ui.h"
#include "oe_core_bridge.h"
#include "oe_audio_backend.h"
#include "oe_input_backend.h"
#include "oe_emulator_view.h"
#include "oe_game_loop.h"

namespace {

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options] [rom_path]\n\n"
              << "Options:\n"
              << "  -c, --core <path>     Path to emulator core .so file\n"
              << "  -d, --db <path>        Path to library database (default: ~/.local/share/openemu/library.db)\n"
              << "  -l, --library          Show library window (default)\n"
              << "  -h, --help             Show this help message\n"
              << std::endl;
}

}

static QString guessSystemFromPath(const QString& romPath)
{
    QString ext = QFileInfo(romPath).suffix().toLower();
    if (ext == "nes") return "org.openemu.nes";
    if (ext == "sfc" || ext == "snes") return "org.openemu.snes";
    if (ext == "gba") return "org.openemu.gba";
    if (ext == "gb" || ext == "gbc") return "org.openemu.gameboy";
    if (ext == "z64" || ext == "n64" || ext == "v64") return "org.openemu.mupen64plus";
    if (ext == "md" || ext == "gen") return "org.openemu.genesis";
    return QString();
}

static QString getCorePathForSystem(const QString& systemId, const QString& romPath = QString())
{
    static const QMap<QString, QString> SYSTEM_CORES = {
        {"org.openemu.nes", "liboe_core_fceu.so"},
        {"org.openemu.fceultra", "liboe_core_fceu.so"},
        {"org.openemu.snes", "liboe_core_snes9x.so"},
        {"org.openemu.gba", "liboe_core_mgba.so"},
        {"org.openemu.gameboy", "liboe_core_mgba.so"},
        {"org.openemu.gameboycolor", "liboe_core_mgba.so"},
        {"org.openemu.mupen64plus", "liboe_core_mgba.so"},
        {"org.openemu.genesis", "liboe_core_mgba.so"},
    };
    
    QString effectiveSystemId = systemId;
    if (effectiveSystemId.isEmpty() && !romPath.isEmpty()) {
        effectiveSystemId = guessSystemFromPath(romPath);
    }
    
    auto it = SYSTEM_CORES.find(effectiveSystemId);
    if (it != SYSTEM_CORES.end()) {
        QString coreName = it.value();
        QString buildDir = QCoreApplication::applicationDirPath();
        QString corePath = buildDir + "/" + coreName;
        if (QFile::exists(corePath)) {
            return corePath;
        }
        corePath = "/home/mike/Desktop/OpenEmu-fork/linux/build/" + coreName;
        if (QFile::exists(corePath)) {
            return corePath;
        }
    }
    return QString();
}

class EmulatorWindow : public QMainWindow {
public:
    EmulatorWindow(const std::string& corePath, const std::string& romPath, QWidget* parent = nullptr)
        : QMainWindow(parent)
        , m_corePath(corePath)
        , m_romPath(romPath)
        , m_initialized(false)
        , m_core(nullptr)
        , m_audio(nullptr)
        , m_input(nullptr)
        , m_view(nullptr)
        , m_gameLoop(nullptr)
    {
        setWindowTitle(QString("OpenEmu Linux - %1").arg(QString::fromStdString(romPath)));
        resize(640, 480);

        setupUI();
        setupInput();
        initEmulator();
    }

    bool isInitialized() const
    {
        return m_initialized;
    }

    ~EmulatorWindow()
    {
        if (m_gameLoop) {
            m_gameLoop->stop();
            m_gameLoop->quit();
            m_gameLoop->wait(1000);
            delete m_gameLoop;
        }
        delete m_input;
        delete m_audio;
    }

private:
    void setupUI()
    {
        QWidget* central = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);

        m_view = new OpenEmu::EmulatorView(central);
        layout->addWidget(m_view);

        setCentralWidget(central);

        QMenu* gameMenu = menuBar()->addMenu("Game");
        QAction* resetAction = gameMenu->addAction("Reset");
        resetAction->setShortcut(QKeySequence(Qt::Key_Escape));
        connect(resetAction, &QAction::triggered, this, [this]() {
            if (m_core) m_core->reset();
        });

        gameMenu->addSeparator();
        QAction* quitAction = gameMenu->addAction("Quit");
        quitAction->setShortcut(QKeySequence::Quit);
        connect(quitAction, &QAction::triggered, this, &QWidget::close);

        statusBar()->showMessage("Ready");
    }

    void setupInput()
    {
        m_input = new OpenEmu::InputBackend();
        m_input->setButtonCallback([this](int player, int button, OpenEmu::InputEvent event) {
            if (m_core) {
                m_core->setButton(player, button, event == OpenEmu::InputEvent::Press);
            }
        });
        m_input->start();
    }

    void initEmulator()
    {
        qDebug() << "initEmulator: core=" << m_corePath.c_str() << "rom=" << m_romPath.c_str();
        try {
            QFileInfo coreInfo(QString::fromStdString(m_corePath));
            QFileInfo romInfo(QString::fromStdString(m_romPath));
            QString resolvedRomPath = romInfo.exists()
                ? romInfo.canonicalFilePath()
                : romInfo.absoluteFilePath();

            if (!coreInfo.exists()) {
                throw std::runtime_error("Core library not found: " + m_corePath);
            }
            if (!romInfo.exists()) {
                throw std::runtime_error("ROM file not found: " + m_romPath);
            }

            qDebug() << "Creating CoreBridge...";
            m_core = std::make_shared<OpenEmu::CoreBridge>(m_corePath);
            qDebug() << "CoreBridge created, loading ROM from:" << resolvedRomPath;
            if (!m_core->loadROM(resolvedRomPath.toStdString())) {
                throw std::runtime_error(
                    "Failed to load ROM\nCore: " + m_corePath +
                    "\nROM: " + resolvedRomPath.toStdString());
            }
            qDebug() << "ROM loaded, setting up audio...";

            int sampleRate = m_core->audioSampleRate();
            m_audio = new OpenEmu::AudioBackend();
            if (!m_audio->start(sampleRate, 2)) {
                std::cerr << "Warning: Failed to start audio, continuing without sound" << std::endl;
                delete m_audio;
                m_audio = nullptr;
            }
            qDebug() << "Audio setup done, getting video size...";

            auto size = m_core->videoSize();
            statusBar()->showMessage(QString("Video: %1x%2 | Audio: %3 Hz")
                .arg(size.width).arg(size.height).arg(sampleRate));
            qDebug() << "Starting game loop...";

            m_gameLoop = new OpenEmu::GameLoop(m_core, m_view, m_audio, m_input);
            m_gameLoop->start();
            m_initialized = true;
            qDebug() << "Game loop started!";

        } catch (const std::exception& e) {
            qDebug() << "Exception:" << e.what();
            QMessageBox::critical(this, "Emulator Error", 
                QString("Failed to initialize emulator:\n%1").arg(e.what()));
            m_initialized = false;
            close();
        }
    }

    std::string m_corePath;
    std::string m_romPath;
    bool m_initialized;
    std::shared_ptr<OpenEmu::CoreBridge> m_core;
    OpenEmu::AudioBackend* m_audio;
    OpenEmu::InputBackend* m_input;
    OpenEmu::EmulatorView* m_view;
    OpenEmu::GameLoop* m_gameLoop;
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("OpenEmu Linux");
    QApplication::setOrganizationName("OpenEmu");

    std::string corePath;
    std::string romPath;
    std::string dbPath;
    bool showLibrary = true;

    static struct option longOptions[] = {
        {"core", required_argument, 0, 'c'},
        {"db", required_argument, 0, 'd'},
        {"library", no_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int optionIndex = 0;
    while ((opt = getopt_long(argc, argv, "c:d:lh", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'c':
                corePath = optarg;
                break;
            case 'd':
                dbPath = optarg;
                break;
            case 'l':
                showLibrary = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        romPath = argv[optind];
        showLibrary = false;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    QString dbFilePath = dbPath.empty() 
        ? QDir::home().filePath(".local/share/openemu/library.db")
        : QString::fromStdString(dbPath);
    
    QDir dbDir = QFileInfo(dbFilePath).dir();
    if (!dbDir.exists()) {
        dbDir.mkpath(".");
    }
    
    auto db = std::make_unique<OpenEmu::LibraryDatabase>(dbFilePath);
    
    if (!db->open()) {
        QMessageBox::critical(nullptr, "Database Error", 
            "Failed to open library database");
        SDL_Quit();
        return 1;
    }
    
    if (!db->migrate()) {
        QMessageBox::critical(nullptr, "Database Error", 
            "Failed to migrate library database schema");
        SDL_Quit();
        return 1;
    }

    if (!showLibrary && !romPath.empty()) {
        if (corePath.empty()) {
            std::cerr << "Error: --core is required when specifying a ROM" << std::endl;
            printUsage(argv[0]);
            SDL_Quit();
            return 1;
        }
        
        EmulatorWindow window(corePath, romPath);
        if (!window.isInitialized()) {
            SDL_Quit();
            return 1;
        }
        window.show();
        int ret = app.exec();
        SDL_Quit();
        return ret;
    }

    OpenEmu::MainWindow mainWindow;
    mainWindow.setLibraryDatabase(db.get());
    
    mainWindow.onRomSelected = [&corePath](const QString& romPath, const QString& systemId) {
        qDebug() << "onRomSelected:" << romPath << "systemId:" << systemId;
        
        QString effectiveCorePath = QString::fromStdString(corePath);
        
        if (effectiveCorePath.isEmpty()) {
            effectiveCorePath = getCorePathForSystem(systemId, romPath);
            qDebug() << "Core from systemId:" << effectiveCorePath;
        }
        
        if (effectiveCorePath.isEmpty()) {
            qDebug() << "System ID didn't match, trying extension fallback...";
            effectiveCorePath = getCorePathForSystem(QString(), romPath);
            qDebug() << "Core from extension:" << effectiveCorePath;
        }
        
        if (effectiveCorePath.isEmpty()) {
            QString guessedSystem = guessSystemFromPath(romPath);
            QMessageBox::critical(nullptr, "No Core",
                QString("No emulator core found for: %1\n\n"
                        "System ID from DB: %2\n"
                        "Guessed system: %3\n\n"
                        "Please pass a core with -c option.")
                    .arg(QFileInfo(romPath).fileName())
                    .arg(systemId.isEmpty() ? "empty" : systemId)
                    .arg(guessedSystem.isEmpty() ? "unknown" : guessedSystem));
            return;
        }
        
        qDebug() << "Starting emulator with core:" << effectiveCorePath << "ROM:" << romPath;
        EmulatorWindow* emuWindow = new EmulatorWindow(effectiveCorePath.toStdString(), romPath.toStdString());
        if (!emuWindow->isInitialized()) {
            emuWindow->deleteLater();
            return;
        }
        emuWindow->setAttribute(Qt::WA_DeleteOnClose);
        emuWindow->show();
    };
    
    mainWindow.show();

    int ret = app.exec();
    SDL_Quit();
    return ret;
}
