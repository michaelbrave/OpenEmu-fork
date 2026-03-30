#include "oe_controller_mapping_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>

namespace OpenEmu {

/* -----------------------------------------------------------------------
 * Static tables
 * ----------------------------------------------------------------------- */

const char* const ControllerMappingDialog::BUTTON_NAMES[OE_NUM_BUTTONS] = {
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
    "A",        "B",          "X",          "Y",
    "L Shoulder","R Shoulder","Start",      "Select"
};

const int ControllerMappingDialog::DEFAULT_KEYS[OE_NUM_BUTTONS] = {
    SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT,
    SDLK_z,  SDLK_x,   SDLK_a,    SDLK_s,
    SDLK_d,  SDLK_f,   SDLK_RETURN, SDLK_SPACE
};

const int ControllerMappingDialog::DEFAULT_BUTTONS[OE_NUM_BUTTONS] = {
    SDL_CONTROLLER_BUTTON_DPAD_UP,      SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_A,            SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,            SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_START,        SDL_CONTROLLER_BUTTON_BACK
};

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

QString ControllerMappingDialog::keyLabel(int sdlKeycode)
{
    if (sdlKeycode == -1) return QStringLiteral("(none)");
    const char* name = SDL_GetKeyName(static_cast<SDL_Keycode>(sdlKeycode));
    return name ? QString::fromUtf8(name) : QStringLiteral("?");
}

QString ControllerMappingDialog::buttonLabel(int sdlButton)
{
    if (sdlButton == -1) return QStringLiteral("(none)");
    const char* name = SDL_GameControllerGetStringForButton(
        static_cast<SDL_GameControllerButton>(sdlButton));
    return name ? QString::fromUtf8(name).toUpper() : QStringLiteral("?");
}

/* -----------------------------------------------------------------------
 * Constructor
 * ----------------------------------------------------------------------- */

ControllerMappingDialog::ControllerMappingDialog(QWidget* parent)
    : QDialog(parent)
    , m_table(new QTableWidget(OE_NUM_BUTTONS, 4, this))
    , m_statusLabel(new QLabel(
          QStringLiteral("Click 'Set' next to a button, then press any key or controller button."),
          this))
    , m_resetBtn(new QPushButton(QStringLiteral("Reset to Defaults"), this))
    , m_pollTimer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("Controller Button Mapping"));
    setMinimumWidth(520);
    setModal(true);

    /* Initialise to defaults, then override from saved settings. */
    for (int i = 0; i < OE_NUM_BUTTONS; i++) {
        m_keys[i]    = DEFAULT_KEYS[i];
        m_buttons[i] = DEFAULT_BUTTONS[i];
    }
    loadFromSettings();

    buildTable();

    m_statusLabel->setStyleSheet(QStringLiteral("color: gray; font-style: italic;"));

    QPushButton* okBtn     = new QPushButton(QStringLiteral("OK"),     this);
    QPushButton* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    okBtn->setDefault(true);

    QHBoxLayout* bottomBtns = new QHBoxLayout;
    bottomBtns->addWidget(m_resetBtn);
    bottomBtns->addStretch();
    bottomBtns->addWidget(cancelBtn);
    bottomBtns->addWidget(okBtn);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_table);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(bottomBtns);

    connect(okBtn,      &QPushButton::clicked, this, &ControllerMappingDialog::accept);
    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);
    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        stopCapture();
        for (int i = 0; i < OE_NUM_BUTTONS; i++) {
            m_keys[i]    = DEFAULT_KEYS[i];
            m_buttons[i] = DEFAULT_BUTTONS[i];
            updateRowDisplay(i);
        }
    });

    m_pollTimer->setInterval(40);
    connect(m_pollTimer, &QTimer::timeout, this, &ControllerMappingDialog::pollForInput);
}

/* -----------------------------------------------------------------------
 * Table
 * ----------------------------------------------------------------------- */

void ControllerMappingDialog::buildTable()
{
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Button"),
         QStringLiteral("Keyboard"),
         QStringLiteral("Controller"),
         QStringLiteral("  ")});

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < OE_NUM_BUTTONS; i++) {
        m_table->setItem(i, 0, new QTableWidgetItem(
            QString::fromUtf8(BUTTON_NAMES[i])));

        m_table->setItem(i, 1, new QTableWidgetItem());
        m_table->setItem(i, 2, new QTableWidgetItem());

        QPushButton* setBtn = new QPushButton(QStringLiteral("Set"), this);
        setBtn->setFixedWidth(48);
        const int row = i;
        connect(setBtn, &QPushButton::clicked, this, [this, row]() {
            onSetClicked(row);
        });
        m_table->setCellWidget(i, 3, setBtn);

        updateRowDisplay(i);
    }
}

void ControllerMappingDialog::updateRowDisplay(int row)
{
    if (row < 0 || row >= OE_NUM_BUTTONS) return;
    m_table->item(row, 1)->setText(keyLabel(m_keys[row]));
    m_table->item(row, 2)->setText(buttonLabel(m_buttons[row]));
}

/* -----------------------------------------------------------------------
 * Capture logic
 * ----------------------------------------------------------------------- */

void ControllerMappingDialog::onSetClicked(int gameButton)
{
    if (m_capturingRow == gameButton) {
        stopCapture();
        return;
    }
    startCapture(gameButton);
}

void ControllerMappingDialog::startCapture(int row)
{
    stopCapture(); /* in case another row was active */
    m_capturingRow = row;

    /*
     * Open every connected device so we receive SDL events for it.
     *
     * SDL_GameControllerOpen gives us abstract SDL_CONTROLLERBUTTONDOWN events
     * (preferred — button indices match our mapping table).  For devices not in
     * SDL's controller database we fall back to SDL_JOYBUTTONDOWN from the raw
     * joystick handle, and try to reverse-map back through the open controller.
     *
     * SDL ref-counts handles, so opening a device that InputBackend already
     * opened is harmless — we just increment the ref count and close it later.
     */
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* ctrl = SDL_GameControllerOpen(i);
            if (ctrl) m_openedControllers.push_back(ctrl);
        } else {
            SDL_Joystick* joy = SDL_JoystickOpen(i);
            if (joy) m_openedJoysticks.push_back(joy);
        }
    }

    /* Change the clicked button text so it's obvious capture is active. */
    if (auto* btn = qobject_cast<QPushButton*>(m_table->cellWidget(row, 3))) {
        btn->setText(QStringLiteral("..."));
    }

    m_statusLabel->setStyleSheet(QStringLiteral("color: #0055cc; font-weight: bold;"));
    m_statusLabel->setText(
        QString("Mapping '%1' — press a key or controller button (ESC to cancel)...")
            .arg(QString::fromUtf8(BUTTON_NAMES[row])));

    m_resetBtn->setEnabled(false);
    m_pollTimer->start();
}

void ControllerMappingDialog::stopCapture()
{
    m_pollTimer->stop();

    if (m_capturingRow >= 0) {
        if (auto* btn = qobject_cast<QPushButton*>(m_table->cellWidget(m_capturingRow, 3))) {
            btn->setText(QStringLiteral("Set"));
        }
        m_capturingRow = -1;
    }

    /* Close the devices we opened for capture. */
    for (SDL_GameController* ctrl : m_openedControllers) SDL_GameControllerClose(ctrl);
    m_openedControllers.clear();
    for (SDL_Joystick* joy : m_openedJoysticks) SDL_JoystickClose(joy);
    m_openedJoysticks.clear();

    m_resetBtn->setEnabled(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray; font-style: italic;"));
    m_statusLabel->setText(
        QStringLiteral("Click 'Set' next to a button, then press any key or controller button."));
}

void ControllerMappingDialog::pollForInput()
{
    if (m_capturingRow < 0) { m_pollTimer->stop(); return; }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {

        case SDL_KEYDOWN: {
            SDL_Keycode key = event.key.keysym.sym;
            if (key == SDLK_ESCAPE) { stopCapture(); return; }
            m_keys[m_capturingRow] = static_cast<int>(key);
            updateRowDisplay(m_capturingRow);
            stopCapture();
            return;
        }

        case SDL_CONTROLLERBUTTONDOWN: {
            /* Preferred path: abstract GameController button. */
            m_buttons[m_capturingRow] = static_cast<int>(event.cbutton.button);
            updateRowDisplay(m_capturingRow);
            stopCapture();
            return;
        }

        case SDL_JOYBUTTONDOWN: {
            /*
             * Fallback: raw joystick button.  Try to find the GameController
             * handle for this joystick instance and ask SDL for the abstract
             * button via SDL_GameControllerGetButtonFromString isn't available,
             * so we iterate our opened controllers to find a match by instance
             * ID and read its buttons via SDL_GameControllerGetButton.
             * If we can map it, store the abstract button; otherwise store
             * the raw index cast to SDL_GameControllerButton so it round-trips.
             */
            SDL_JoystickID which = event.jbutton.which;
            int rawBtn = event.jbutton.button;
            int abstractBtn = -1;

            for (SDL_GameController* ctrl : m_openedControllers) {
                SDL_Joystick* joy = SDL_GameControllerGetJoystick(ctrl);
                if (joy && SDL_JoystickInstanceID(joy) == which) {
                    /* Scan abstract buttons to find which one is pressed. */
                    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
                        if (SDL_GameControllerGetButton(ctrl,
                                static_cast<SDL_GameControllerButton>(b))) {
                            abstractBtn = b;
                            break;
                        }
                    }
                    break;
                }
            }

            m_buttons[m_capturingRow] = (abstractBtn >= 0) ? abstractBtn : rawBtn;
            updateRowDisplay(m_capturingRow);
            stopCapture();
            return;
        }

        default:
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * Settings persistence
 * ----------------------------------------------------------------------- */

void ControllerMappingDialog::loadFromSettings()
{
    QSettings s(QStringLiteral("OpenEmu"), QStringLiteral("Linux"));
    s.beginGroup(QStringLiteral("controls"));
    if (s.contains(QStringLiteral("key_0"))) {
        for (int i = 0; i < OE_NUM_BUTTONS; i++) {
            m_keys[i]    = s.value(QString("key_%1").arg(i), m_keys[i]).toInt();
            m_buttons[i] = s.value(QString("btn_%1").arg(i), m_buttons[i]).toInt();
        }
    }
    s.endGroup();
}

void ControllerMappingDialog::saveToSettings() const
{
    QSettings s(QStringLiteral("OpenEmu"), QStringLiteral("Linux"));
    s.beginGroup(QStringLiteral("controls"));
    for (int i = 0; i < OE_NUM_BUTTONS; i++) {
        s.setValue(QString("key_%1").arg(i), m_keys[i]);
        s.setValue(QString("btn_%1").arg(i), m_buttons[i]);
    }
    s.endGroup();
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void ControllerMappingDialog::loadFromBackend(const InputBackend& backend)
{
    backend.getCustomMappings(m_keys, m_buttons);
    for (int i = 0; i < OE_NUM_BUTTONS; i++) updateRowDisplay(i);
}

void ControllerMappingDialog::accept()
{
    stopCapture();
    saveToSettings();
    QDialog::accept();
}

} // namespace OpenEmu
