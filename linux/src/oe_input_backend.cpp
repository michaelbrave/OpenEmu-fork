#include "oe_input_backend.h"
#include <iostream>
#include <cstdlib>

namespace OpenEmu {

static constexpr Sint16 AXIS_DEADZONE = 16000;
static bool shouldLogInputEvents()
{
    static const bool enabled = std::getenv("OPENEMU_DEBUG_INPUT") != nullptr;
    return enabled;
}

InputBackend::InputBackend()
    : m_nextPlayer(1)
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL_InitSubSystem(GAMECONTROLLER) failed: " << SDL_GetError() << std::endl;
    }

    SDL_GameControllerEventState(SDL_ENABLE);

    // Default keyboard mapping for Player 0
    m_keyMap[SDLK_UP]     = 0;  // Up
    m_keyMap[SDLK_DOWN]   = 1;  // Down
    m_keyMap[SDLK_LEFT]   = 2;  // Left
    m_keyMap[SDLK_RIGHT]  = 3;  // Right
    m_keyMap[SDLK_z]      = 4;  // A
    m_keyMap[SDLK_x]      = 5;  // B
    m_keyMap[SDLK_a]      = 6;  // X
    m_keyMap[SDLK_s]      = 7;  // Y
    m_keyMap[SDLK_d]      = 8;  // L
    m_keyMap[SDLK_f]      = 9;  // R
    m_keyMap[SDLK_RETURN] = 10; // Start
    m_keyMap[SDLK_SPACE]  = 11; // Select

    // Default controller button mapping (matches SNES layout)
    m_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_UP]      = 0;
    m_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_DOWN]    = 1;
    m_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_LEFT]    = 2;
    m_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]   = 3;
    m_buttonMap[SDL_CONTROLLER_BUTTON_A]             = 4;
    m_buttonMap[SDL_CONTROLLER_BUTTON_B]             = 5;
    m_buttonMap[SDL_CONTROLLER_BUTTON_X]             = 6;
    m_buttonMap[SDL_CONTROLLER_BUTTON_Y]             = 7;
    m_buttonMap[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]  = 8;
    m_buttonMap[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = 9;
    m_buttonMap[SDL_CONTROLLER_BUTTON_START]         = 10;
    m_buttonMap[SDL_CONTROLLER_BUTTON_BACK]          = 11;

    m_axisMap[SDL_CONTROLLER_AXIS_LEFTX] = {2, 3}; // Left, Right
    m_axisMap[SDL_CONTROLLER_AXIS_LEFTY] = {0, 1}; // Up, Down
}

InputBackend::~InputBackend()
{
    stop();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

bool InputBackend::start()
{
    // Scan for already-connected controllers
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            handleControllerAdded(i);
        }
    }
    return true;
}

void InputBackend::stop()
{
    for (auto& pair : m_controllerMap) {
        SDL_GameControllerClose(pair.second);
    }
    m_controllerMap.clear();
    m_joystickMap.clear();
    m_axisButtonState.clear();
    m_nextPlayer = 1;
}

void InputBackend::setButtonCallback(ButtonCallback callback)
{
    m_callback = callback;
}

void InputBackend::handleControllerAdded(int deviceIndex)
{
    if (!SDL_IsGameController(deviceIndex)) return;

    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        std::cerr << "SDL_GameControllerOpen(" << deviceIndex << ") failed: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_JoystickID joystickId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    if (m_controllerMap.find(joystickId) != m_controllerMap.end()) {
        SDL_GameControllerClose(controller);
        return;
    }

    int player = m_joystickMap.empty() ? 0 : m_nextPlayer++;
    m_joystickMap[joystickId] = player;
    m_controllerMap[joystickId] = controller;

    const char* name = SDL_GameControllerName(controller);
    std::cout << "Controller connected: " << (name ? name : "Unknown") << " (Player " << player << ")" << std::endl;
}

void InputBackend::handleControllerRemoved(SDL_JoystickID joystickId)
{
    auto it = m_controllerMap.find(joystickId);
    if (it != m_controllerMap.end()) {
        SDL_GameControllerClose(it->second);
        int player = m_joystickMap[joystickId];
        std::cout << "Controller disconnected (Player " << player << ")" << std::endl;
        m_controllerMap.erase(it);
        m_joystickMap.erase(joystickId);
    }

    for (auto itState = m_axisButtonState.begin(); itState != m_axisButtonState.end();) {
        if (itState->first.first == joystickId) {
            itState = m_axisButtonState.erase(itState);
        } else {
            ++itState;
        }
    }
}

void InputBackend::handleAxisMotion(SDL_JoystickID joystickId, SDL_GameControllerAxis axis, Sint16 value)
{
    auto playerIt = m_joystickMap.find(joystickId);
    if (playerIt == m_joystickMap.end() || !m_callback) return;

    auto axisIt = m_axisMap.find(axis);
    if (axisIt == m_axisMap.end()) return;

    const int player = playerIt->second;
    const int negativeButton = axisIt->second.first;
    const int positiveButton = axisIt->second.second;

    const bool negativePressed = value <= -AXIS_DEADZONE;
    const bool positivePressed = value >= AXIS_DEADZONE;

    const std::pair<SDL_JoystickID, int> negativeKey{joystickId, negativeButton};
    const std::pair<SDL_JoystickID, int> positiveKey{joystickId, positiveButton};

    const bool wasNegativePressed = m_axisButtonState[negativeKey];
    const bool wasPositivePressed = m_axisButtonState[positiveKey];

    if (negativePressed != wasNegativePressed) {
        m_axisButtonState[negativeKey] = negativePressed;
        m_callback(player, negativeButton, negativePressed ? InputEvent::Press : InputEvent::Release);
    }

    if (positivePressed != wasPositivePressed) {
        m_axisButtonState[positiveKey] = positivePressed;
        m_callback(player, positiveButton, positivePressed ? InputEvent::Press : InputEvent::Release);
    }
}

void InputBackend::pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            if (event.key.repeat) break;
            auto it = m_keyMap.find(event.key.keysym.sym);
            if (it != m_keyMap.end() && m_callback) {
                m_callback(0, it->second,
                           event.type == SDL_KEYDOWN ? InputEvent::Press : InputEvent::Release);
            }
            break;
        }
        case SDL_CONTROLLERDEVICEADDED:
            handleControllerAdded(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            handleControllerRemoved(event.cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            auto it = m_joystickMap.find(event.cbutton.which);
            if (it == m_joystickMap.end()) break;
            int player = it->second;

            SDL_GameControllerButton button = static_cast<SDL_GameControllerButton>(event.cbutton.button);
            if (shouldLogInputEvents()) {
                std::cout << "SDL controller button player=" << player
                          << " which=" << event.cbutton.which
                          << " button=" << static_cast<int>(button)
                          << " state=" << (event.type == SDL_CONTROLLERBUTTONDOWN ? "down" : "up")
                          << std::endl;
            }
            auto mapIt = m_buttonMap.find(button);
            if (mapIt != m_buttonMap.end() && m_callback) {
                m_callback(player, mapIt->second,
                           event.type == SDL_CONTROLLERBUTTONDOWN ? InputEvent::Press : InputEvent::Release);
            }
            break;
        }
        case SDL_CONTROLLERAXISMOTION:
            handleAxisMotion(event.caxis.which,
                             static_cast<SDL_GameControllerAxis>(event.caxis.axis),
                             event.caxis.value);
            break;
        default:
            break;
        }
    }
}

void InputBackend::getCustomMappings(int keys[OE_NUM_BUTTONS], int buttons[OE_NUM_BUTTONS]) const
{
    for (int i = 0; i < OE_NUM_BUTTONS; i++) {
        keys[i]    = -1;
        buttons[i] = -1;
    }
    for (const auto& pair : m_keyMap) {
        int btn = pair.second;
        if (btn >= 0 && btn < OE_NUM_BUTTONS) keys[btn] = static_cast<int>(pair.first);
    }
    for (const auto& pair : m_buttonMap) {
        int btn = pair.second;
        if (btn >= 0 && btn < OE_NUM_BUTTONS) buttons[btn] = static_cast<int>(pair.first);
    }
}

void InputBackend::setCustomMappings(const int keys[OE_NUM_BUTTONS], const int buttons[OE_NUM_BUTTONS])
{
    m_keyMap.clear();
    m_buttonMap.clear();
    for (int i = 0; i < OE_NUM_BUTTONS; i++) {
        if (keys[i] != -1)
            m_keyMap[static_cast<SDL_Keycode>(keys[i])] = i;
        if (buttons[i] != -1)
            m_buttonMap[static_cast<SDL_GameControllerButton>(buttons[i])] = i;
    }
}

} // namespace OpenEmu
