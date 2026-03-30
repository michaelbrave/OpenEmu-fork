#ifndef OE_CONTROLLER_MAPPING_DIALOG_H
#define OE_CONTROLLER_MAPPING_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <SDL2/SDL.h>
#include <vector>
#include "oe_input_backend.h"

namespace OpenEmu {

/*
 * ControllerMappingDialog
 *
 * Shows a table of the 12 generic game buttons (D-pad, face, shoulders,
 * Start/Select) with their current keyboard and controller assignments.
 * Clicking "Set" next to any row enters capture mode: the next physical
 * key press or controller button press is recorded as that button's binding.
 *
 * Mappings are loaded from / saved to QSettings("OpenEmu","Linux") under
 * the "controls" group.  Call InputBackend::setCustomMappings() with the
 * saved arrays to apply them at game-launch time.
 */
class ControllerMappingDialog : public QDialog {
    Q_OBJECT
public:
    explicit ControllerMappingDialog(QWidget* parent = nullptr);

    /* Optionally pre-load live mappings from a running InputBackend. */
    void loadFromBackend(const InputBackend& backend);

    void accept() override;

private slots:
    void onSetClicked(int gameButton);
    void pollForInput();

private:
    static const char* const BUTTON_NAMES[OE_NUM_BUTTONS];
    static const int DEFAULT_KEYS[OE_NUM_BUTTONS];
    static const int DEFAULT_BUTTONS[OE_NUM_BUTTONS];

    void buildTable();
    void loadFromSettings();
    void saveToSettings() const;
    void updateRowDisplay(int row);
    void startCapture(int row);
    void stopCapture();

    static QString keyLabel(int sdlKeycode);
    static QString buttonLabel(int sdlButton);

    QTableWidget* m_table;
    QLabel*       m_statusLabel;
    QPushButton*  m_resetBtn;
    QTimer*       m_pollTimer;

    int m_capturingRow = -1;

    /*
     * Controllers and joysticks opened by this dialog for event capture.
     * Stored so they can be closed when capture ends.
     */
    std::vector<SDL_GameController*> m_openedControllers;
    std::vector<SDL_Joystick*>       m_openedJoysticks;

    /* Current assignments: SDL_Keycode / SDL_GameControllerButton, or -1. */
    int m_keys[OE_NUM_BUTTONS];
    int m_buttons[OE_NUM_BUTTONS];
};

} // namespace OpenEmu

#endif // OE_CONTROLLER_MAPPING_DIALOG_H
