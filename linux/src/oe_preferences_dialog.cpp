#include "oe_preferences_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QSettings>

namespace OpenEmu {

static const QString SETTINGS_ORG = QStringLiteral("OpenEmu");
static const QString SETTINGS_APP = QStringLiteral("Linux");

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
    , m_tabs(new QTabWidget(this))
    , m_pathsList(new QListWidget(this))
    , m_addPathBtn(new QPushButton("Add...", this))
    , m_removePathBtn(new QPushButton("Remove", this))
    , m_apiKeyEdit(new QLineEdit(this))
    , m_clearCacheBtn(new QPushButton("Clear Cover Cache", this))
    , m_frameskipBox(new QSpinBox(this))
    , m_sampleRateBox(new QSpinBox(this))
    , m_shaderEdit(new QLineEdit(this))
    , m_browseShaderBtn(new QPushButton("Browse...", this))
    , m_vsyncCheck(new QCheckBox("Enable V-Sync", this))
    , m_controllerList(new QListWidget(this))
    , m_controllerCombo(new QComboBox(this))
{
    setWindowTitle("Preferences");
    setMinimumSize(500, 450);
    setModal(true);
    
    setupPathsTab();
    setupCoverArtTab();
    setupEmulationTab();
    setupInputTab();
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tabs);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    QPushButton* okBtn = new QPushButton("OK", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &PreferencesDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    mainLayout->addLayout(buttonLayout);
    
    loadSettings();
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::setupPathsTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QLabel* descLabel = new QLabel("Configure directories where OpenEmu should search for ROM files:");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    layout->addWidget(m_pathsList);
    
    QHBoxLayout* pathButtons = new QHBoxLayout;
    pathButtons->addWidget(m_addPathBtn);
    pathButtons->addWidget(m_removePathBtn);
    pathButtons->addStretch();
    layout->addLayout(pathButtons);
    
    layout->addStretch();
    
    connect(m_addPathBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select ROM Directory");
        if (!dir.isEmpty()) {
            m_pathsList->addItem(dir);
        }
    });
    
    connect(m_removePathBtn, &QPushButton::clicked, this, [this]() {
        delete m_pathsList->currentItem();
    });
    
    m_tabs->addTab(tab, "ROM Paths");
}

void PreferencesDialog::setupCoverArtTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QGroupBox* apiBox = new QGroupBox("TheGamesDB API");
    QFormLayout* apiLayout = new QFormLayout(apiBox);
    
    QLabel* keyDesc = new QLabel("Get a free API key from <a href='https://forums.thegamesdb.net/viewforum.php?f=10'>TheGamesDB forums</a>");
    keyDesc->setOpenExternalLinks(true);
    apiLayout->addRow("API Key:", m_apiKeyEdit);
    apiLayout->addRow("", keyDesc);
    
    layout->addWidget(apiBox);
    
    QGroupBox* cacheBox = new QGroupBox("Cache");
    QVBoxLayout* cacheLayout = new QVBoxLayout(cacheBox);
    cacheLayout->addWidget(m_clearCacheBtn);
    
    QLabel* cachePathLabel = new QLabel(QString("Cache locations: %1 and %2").arg(
        QDir::home().filePath(".local/share/openemu/gamesdb"),
        QDir::home().filePath(".local/share/openemu/covers")));
    cachePathLabel->setStyleSheet("color: gray;");
    cacheLayout->addWidget(cachePathLabel);
    
    layout->addWidget(cacheBox);
    
    layout->addStretch();
    
    connect(m_clearCacheBtn, &QPushButton::clicked, this, [this, cachePathLabel]() {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Clear Cache", "This will delete all cached cover art. Continue?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            QDir metadataCacheDir(QDir::home().filePath(".local/share/openemu/gamesdb"));
            metadataCacheDir.removeRecursively();
            QDir coverCacheDir(QDir::home().filePath(".local/share/openemu/covers"));
            coverCacheDir.removeRecursively();
            QMessageBox::information(this, "Cache Cleared", "Cover art cache has been cleared.");
        }
    });
    
    m_tabs->addTab(tab, "Cover Art");
}

void PreferencesDialog::setupEmulationTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QGroupBox* videoBox = new QGroupBox("Video");
    QFormLayout* videoLayout = new QFormLayout(videoBox);
    
    m_frameskipBox->setRange(0, 10);
    m_frameskipBox->setSuffix(" frames");
    videoLayout->addRow("Frameskip:", m_frameskipBox);
    
    QHBoxLayout* shaderLayout = new QHBoxLayout;
    shaderLayout->addWidget(m_shaderEdit);
    shaderLayout->addWidget(m_browseShaderBtn);
    videoLayout->addRow("Shader:", shaderLayout);
    
    videoLayout->addRow(m_vsyncCheck);
    
    layout->addWidget(videoBox);
    
    QGroupBox* audioBox = new QGroupBox("Audio");
    QFormLayout* audioLayout = new QFormLayout(audioBox);
    
    m_sampleRateBox->setRange(8000, 192000);
    m_sampleRateBox->setSingleStep(1000);
    m_sampleRateBox->setSuffix(" Hz");
    m_sampleRateBox->setValue(48000);
    audioLayout->addRow("Sample Rate:", m_sampleRateBox);
    
    layout->addWidget(audioBox);
    
    layout->addStretch();
    
    connect(m_browseShaderBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Shader",
            QString(), "GLSL Files (*.glsl *.frag *.vert);;All Files (*)");
        if (!file.isEmpty()) {
            m_shaderEdit->setText(file);
        }
    });
    
    m_tabs->addTab(tab, "Emulation");
}

void PreferencesDialog::setupInputTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    QGroupBox* controllerBox = new QGroupBox("Game Controller");
    QFormLayout* controllerLayout = new QFormLayout(controllerBox);
    
    m_controllerCombo->addItem("Auto-detect");
    m_controllerCombo->addItem("Keyboard Only");
    m_controllerCombo->addItem("Xbox Controller");
    m_controllerCombo->addItem("PlayStation Controller");
    m_controllerCombo->addItem("Generic Gamepad");
    controllerLayout->addRow("Controller Type:", m_controllerCombo);
    
    layout->addWidget(controllerBox);
    
    QGroupBox* mappingBox = new QGroupBox("Button Mapping");
    QVBoxLayout* mappingLayout = new QVBoxLayout(mappingBox);
    
    QStringList defaultMappings = {
        "D-Pad Up    : Keyboard Up / Axis Left Y-",
        "D-Pad Down  : Keyboard Down / Axis Left Y+",
        "D-Pad Left  : Keyboard Left / Axis Left X-",
        "D-Pad Right : Keyboard Right / Axis Left X+",
        "A / B       : Keyboard Z / Keyboard X",
        "Start       : Keyboard Enter",
        "Select      : Keyboard Backspace",
        "L Shoulder  : Keyboard Q",
        "R Shoulder  : Keyboard W",
    };
    
    m_controllerList->addItems(defaultMappings);
    mappingLayout->addWidget(m_controllerList);
    
    QHBoxLayout* mappingButtons = new QHBoxLayout;
    QPushButton* defaultsBtn = new QPushButton("Reset to Defaults", this);
    QPushButton* calibrateBtn = new QPushButton("Calibrate...", this);
    mappingButtons->addWidget(defaultsBtn);
    mappingButtons->addWidget(calibrateBtn);
    mappingButtons->addStretch();
    mappingLayout->addLayout(mappingButtons);
    
    layout->addWidget(mappingBox);
    
    QLabel* hintLabel = new QLabel("Tip: Press a key or button while hovering over a mapping to change it.");
    hintLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(hintLabel);
    
    layout->addStretch();
    
    m_tabs->addTab(tab, "Input");
}

void PreferencesDialog::loadSettings()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    
    QStringList paths = settings.value("romPaths").toStringList();
    m_pathsList->clear();
    for (const QString& path : paths) {
        m_pathsList->addItem(path);
    }
    
    m_apiKeyEdit->setText(settings.value("tgdbApiKey").toString());
    
    m_frameskipBox->setValue(settings.value("frameskip", 0).toInt());
    m_sampleRateBox->setValue(settings.value("sampleRate", 48000).toInt());
    m_shaderEdit->setText(settings.value("shader").toString());
    m_vsyncCheck->setChecked(settings.value("vsync", true).toBool());
}

void PreferencesDialog::saveSettings()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    
    QStringList paths;
    for (int i = 0; i < m_pathsList->count(); ++i) {
        paths << m_pathsList->item(i)->text();
    }
    settings.setValue("romPaths", paths);
    
    settings.setValue("tgdbApiKey", m_apiKeyEdit->text());
    
    settings.setValue("frameskip", m_frameskipBox->value());
    settings.setValue("sampleRate", m_sampleRateBox->value());
    settings.setValue("shader", m_shaderEdit->text());
    settings.setValue("vsync", m_vsyncCheck->isChecked());
}

void PreferencesDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

QStringList PreferencesDialog::romPaths() const
{
    QStringList paths;
    for (int i = 0; i < m_pathsList->count(); ++i) {
        paths << m_pathsList->item(i)->text();
    }
    return paths;
}

QString PreferencesDialog::apiKey() const
{
    return m_apiKeyEdit->text();
}

int PreferencesDialog::frameskip() const
{
    return m_frameskipBox->value();
}

bool PreferencesDialog::vsync() const
{
    return m_vsyncCheck->isChecked();
}

QString PreferencesDialog::shader() const
{
    return m_shaderEdit->text();
}

} // namespace OpenEmu
