#ifndef OE_PREFERENCES_DIALOG_H
#define OE_PREFERENCES_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QStringList>

namespace OpenEmu {

class PreferencesDialog : public QDialog {
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog();

    QStringList romPaths() const;
    QString apiKey() const;
    int frameskip() const;
    bool vsync() const;
    QString shader() const;

    void accept() override;

private:
    void setupPathsTab();
    void setupCoverArtTab();
    void setupEmulationTab();
    void setupInputTab();
    void loadSettings();
    void saveSettings();

    QTabWidget* m_tabs;
    
    QListWidget* m_pathsList;
    QPushButton* m_addPathBtn;
    QPushButton* m_removePathBtn;
    
    QLineEdit* m_apiKeyEdit;
    QPushButton* m_clearCacheBtn;
    
    QSpinBox* m_frameskipBox;
    QSpinBox* m_sampleRateBox;
    QLineEdit* m_shaderEdit;
    QPushButton* m_browseShaderBtn;
    QCheckBox* m_vsyncCheck;
    
    QListWidget* m_controllerList;
    QComboBox* m_controllerCombo;
};

} // namespace OpenEmu

#endif // OE_PREFERENCES_DIALOG_H
