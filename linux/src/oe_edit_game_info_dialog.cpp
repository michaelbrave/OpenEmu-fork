#include "oe_edit_game_info_dialog.h"
#include <QDialogButtonBox>

namespace OpenEmu {

EditGameInfoDialog::EditGameInfoDialog(QWidget* parent)
    : QDialog(parent)
    , m_titleEdit(new QLineEdit(this))
    , m_systemEdit(new QLineEdit(this))
    , m_pathLabel(new QLabel(this))
{
    setWindowTitle("Edit Game Info");
    setMinimumWidth(400);
    setModal(true);
    setupUI();
}

EditGameInfoDialog::~EditGameInfoDialog() = default;

void EditGameInfoDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout;
    
    m_titleEdit->setPlaceholderText("Game title");
    m_systemEdit->setPlaceholderText("System identifier");
    m_pathLabel->setStyleSheet("color: #888;");
    
    formLayout->addRow("Title:", m_titleEdit);
    formLayout->addRow("System ID:", m_systemEdit);
    formLayout->addRow("Path:", m_pathLabel);
    
    mainLayout->addLayout(formLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttons);
}

void EditGameInfoDialog::setGameInfo(const QString& title, const QString& system, const QString& path)
{
    m_titleEdit->setText(title);
    m_systemEdit->setText(system);
    m_pathLabel->setText(path);
    m_pathLabel->setWordWrap(true);
}

QString EditGameInfoDialog::editedTitle() const
{
    return m_titleEdit->text().trimmed();
}

QString EditGameInfoDialog::editedSystem() const
{
    return m_systemEdit->text().trimmed();
}

} // namespace OpenEmu
