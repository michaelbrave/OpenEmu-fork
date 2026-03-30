#ifndef OE_GAME_LOOP_H
#define OE_GAME_LOOP_H

#include <QThread>
#include <QAtomicInt>
#include <memory>
#include "oe_core_bridge.h"
#include "oe_emulator_view.h"
#include "oe_audio_backend.h"
#include "oe_input_backend.h"

namespace OpenEmu {

class GameLoop : public QThread {
public:
    GameLoop(std::shared_ptr<CoreBridge> core, EmulatorView* view, AudioBackend* audio, InputBackend* input);
    ~GameLoop();

    void stop();

protected:
    void run() override;

private:
    std::shared_ptr<CoreBridge> m_core;
    EmulatorView* m_view;
    AudioBackend* m_audio;
    InputBackend* m_input;
    QAtomicInt m_running;
};

} // namespace OpenEmu

#endif // OE_GAME_LOOP_H
