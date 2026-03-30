#ifndef OE_COVER_ART_DIALOG_H
#define OE_COVER_ART_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QPushButton>

namespace OpenEmu {

class CoverArtDialog : public QDialog {
public:
    explicit CoverArtDialog(QWidget* parent = nullptr);
    ~CoverArtDialog();

    void setGameInfo(const QString& title, const QString& systemName);
    QPixmap selectedCover() const;

private:
    void setupUI();
    void setPreviewPixmap(const QPixmap& pixmap);

    QString m_gameTitle;
    QString m_systemName;
    QLabel* m_titleLabel;
    QLabel* m_previewLabel;
    QLineEdit* m_urlEdit;
    QPushButton* m_okButton;
    QNetworkAccessManager* m_networkManager;
    QPixmap m_selectedCover;
};

} // namespace OpenEmu

#endif // OE_COVER_ART_DIALOG_H
