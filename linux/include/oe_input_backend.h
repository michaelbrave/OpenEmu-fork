#ifndef OE_INPUT_BACKEND_H
#define OE_INPUT_BACKEND_H

#include <SDL2/SDL.h>
#include <functional>
#include <map>
#include <utility>

namespace OpenEmu {

enum class InputEvent {
    Press,
    Release
};

static const int OE_NUM_BUTTONS = 12;

class InputBackend {
public:
    typedef std::function<void(int player, int button, InputEvent event)> ButtonCallback;

    InputBackend();
    ~InputBackend();

    bool start();
    void stop();

    void pollEvents();
    void setButtonCallback(ButtonCallback callback);

    /*
     * Mapping accessors.
     * keys[i]    = SDL_Keycode assigned to game button i, or -1 if unset.
     * buttons[i] = SDL_GameControllerButton assigned to game button i, or -1.
     */
    void getCustomMappings(int keys[OE_NUM_BUTTONS], int buttons[OE_NUM_BUTTONS]) const;
    void setCustomMappings(const int keys[OE_NUM_BUTTONS], const int buttons[OE_NUM_BUTTONS]);

private:
    void handleAxisMotion(SDL_JoystickID joystickId, SDL_GameControllerAxis axis, Sint16 value);
    void handleControllerAdded(int deviceIndex);
    void handleControllerRemoved(SDL_JoystickID joystickId);

    ButtonCallback m_callback;
    std::map<SDL_Keycode, int> m_keyMap;
    std::map<SDL_GameControllerButton, int> m_buttonMap;
    std::map<SDL_GameControllerAxis, std::pair<int, int>> m_axisMap;
    std::map<SDL_JoystickID, int> m_joystickMap;    // joystick ID -> player index
    std::map<SDL_JoystickID, SDL_GameController*> m_controllerMap; // joystick ID -> controller handle
    std::map<std::pair<SDL_JoystickID, int>, bool> m_axisButtonState;
    int m_nextPlayer;
};

} // namespace OpenEmu

#endif // OE_INPUT_BACKEND_H
