#ifndef OE_INPUT_BACKEND_H
#define OE_INPUT_BACKEND_H

#include <SDL2/SDL.h>
#include <functional>
#include <map>

namespace OpenEmu {

enum class InputEvent {
    Press,
    Release
};

class InputBackend {
public:
    typedef std::function<void(int player, int button, InputEvent event)> ButtonCallback;

    InputBackend();
    ~InputBackend();

    bool start();
    void stop();

    void pollEvents();
    void setButtonCallback(ButtonCallback callback);

private:
    void handleControllerAdded(int deviceIndex);
    void handleControllerRemoved(SDL_JoystickID joystickId);

    ButtonCallback m_callback;
    std::map<SDL_Keycode, int> m_keyMap;
    std::map<SDL_GameControllerButton, int> m_buttonMap;
    std::map<SDL_JoystickID, int> m_joystickMap;    // joystick ID -> player index
    std::map<SDL_JoystickID, SDL_GameController*> m_controllerMap; // joystick ID -> controller handle
    int m_nextPlayer;
};

} // namespace OpenEmu

#endif // OE_INPUT_BACKEND_H
