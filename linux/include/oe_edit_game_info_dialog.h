#ifndef OE_EDIT_GAME_INFO_DIALOG_H
#define OE_EDIT_GAME_INFO_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>

namespace OpenEmu {

class EditGameInfoDialog : public QDialog {
public:
    explicit EditGameInfoDialog(QWidget* parent = nullptr);
    ~EditGameInfoDialog();

    void setGameInfo(const QString& title, const QString& system, const QString& path);
    QString editedTitle() const;
    QString editedSystem() const;

private:
    void setupUI();

    QLineEdit* m_titleEdit;
    QLineEdit* m_systemEdit;
    QLabel* m_pathLabel;
};

} // namespace OpenEmu

#endif // OE_EDIT_GAME_INFO_DIALOG_H
