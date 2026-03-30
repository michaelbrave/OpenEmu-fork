#include "oe_input_backend.h"
#include <iostream>

namespace OpenEmu {

InputBackend::InputBackend()
    : m_nextPlayer(1) // Player 0 is reserved for keyboard
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL_InitSubSystem(GAMECONTROLLER) failed: " << SDL_GetError() << std::endl;
    }

    SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG, "1");
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

    int player = m_nextPlayer++;
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
            auto mapIt = m_buttonMap.find(button);
            if (mapIt != m_buttonMap.end() && m_callback) {
                m_callback(player, mapIt->second,
                           event.type == SDL_CONTROLLERBUTTONDOWN ? InputEvent::Press : InputEvent::Release);
            }
            break;
        }
        default:
            break;
        }
    }
}

} // namespace OpenEmu
