#include "oe_cover_art_dialog.h"

#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace OpenEmu {

CoverArtDialog::CoverArtDialog(QWidget* parent)
    : QDialog(parent)
    , m_titleLabel(new QLabel(this))
    , m_previewLabel(new QLabel(this))
    , m_urlEdit(new QLineEdit(this))
    , m_okButton(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
{
    setWindowTitle("Set Cover Art");
    setMinimumWidth(520);
    setModal(true);
    setupUI();
}

CoverArtDialog::~CoverArtDialog() = default;

void CoverArtDialog::setGameInfo(const QString& title, const QString& systemName)
{
    m_gameTitle = title;
    m_systemName = systemName;
    m_titleLabel->setText(QString("%1\n%2").arg(title, systemName));
    m_urlEdit->setText(QString());
    m_selectedCover = QPixmap();
    setPreviewPixmap(QPixmap());
    if (m_okButton) {
        m_okButton->setEnabled(false);
    }
}

QPixmap CoverArtDialog::selectedCover() const
{
    return m_selectedCover;
}

void CoverArtDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_titleLabel->setWordWrap(true);
    mainLayout->addWidget(m_titleLabel);

    m_previewLabel->setFixedSize(220, 220);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("border: 1px solid #555; background: #111; color: #999;");
    mainLayout->addWidget(m_previewLabel, 0, Qt::AlignHCenter);

    QLabel* hint = new QLabel(
        "Choose a local image, paste an image URL, or open a web search for this game and then paste the image URL.");
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    QHBoxLayout* actionLayout = new QHBoxLayout;
    QPushButton* chooseFileButton = new QPushButton("Choose File...", this);
    QPushButton* webSearchButton = new QPushButton("Search Web", this);
    actionLayout->addWidget(chooseFileButton);
    actionLayout->addWidget(webSearchButton);
    mainLayout->addLayout(actionLayout);

    QFormLayout* urlLayout = new QFormLayout;
    m_urlEdit->setPlaceholderText("https://example.com/cover.png");
    QPushButton* loadUrlButton = new QPushButton("Load URL", this);

    QWidget* urlRow = new QWidget(this);
    QHBoxLayout* urlRowLayout = new QHBoxLayout(urlRow);
    urlRowLayout->setContentsMargins(0, 0, 0, 0);
    urlRowLayout->addWidget(m_urlEdit);
    urlRowLayout->addWidget(loadUrlButton);
    urlLayout->addRow("Image URL:", urlRow);
    mainLayout->addLayout(urlLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    connect(chooseFileButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this,
            "Choose Cover Art",
            QString(),
            "Images (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*)");
        if (path.isEmpty()) return;

        QPixmap pixmap(path);
        if (pixmap.isNull()) {
            QMessageBox::warning(this, "Invalid Image", "Failed to load the selected image file.");
            return;
        }

        setPreviewPixmap(pixmap);
    });

    connect(webSearchButton, &QPushButton::clicked, this, [this]() {
        QUrl url("https://www.google.com/search");
        QUrlQuery query;
        query.addQueryItem("tbm", "isch");
        query.addQueryItem("q", QString("%1 %2 box art").arg(m_gameTitle, m_systemName));
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });

    connect(loadUrlButton, &QPushButton::clicked, this, [this]() {
        const QUrl url = QUrl::fromUserInput(m_urlEdit->text().trimmed());
        if (!url.isValid() || url.scheme().isEmpty()) {
            QMessageBox::warning(this, "Invalid URL", "Enter a valid image URL.");
            return;
        }

        QNetworkReply* reply = m_networkManager->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::warning(this, "Image Download Failed", reply->errorString());
                return;
            }

            QPixmap pixmap;
            pixmap.loadFromData(reply->readAll());
            if (pixmap.isNull()) {
                QMessageBox::warning(this, "Invalid Image", "The downloaded data is not a supported image.");
                return;
            }

            setPreviewPixmap(pixmap);
        });
    });
}

void CoverArtDialog::setPreviewPixmap(const QPixmap& pixmap)
{
    m_selectedCover = pixmap;
    if (pixmap.isNull()) {
        m_previewLabel->setText("No cover selected");
    } else {
        m_previewLabel->setPixmap(
            pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (m_okButton) {
        m_okButton->setEnabled(!pixmap.isNull());
    }
}

} // namespace OpenEmu
