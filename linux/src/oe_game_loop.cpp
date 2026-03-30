#include "oe_game_loop.h"
#include <QTimer>
#include <chrono>
#include <thread>
#include <iostream>

namespace OpenEmu {

static const int AUDIO_BUFFER_SAMPLES = 2048;

GameLoop::GameLoop(std::shared_ptr<CoreBridge> core, EmulatorView* view, AudioBackend* audio, InputBackend* input)
    : QThread()
    , m_core(core)
    , m_view(view)
    , m_audio(audio)
    , m_input(input)
    , m_running(1)
{
}

GameLoop::~GameLoop()
{
    stop();
    wait();
}

void GameLoop::stop()
{
    m_running = 0;
}

void GameLoop::run()
{
    if (!m_core || !m_view) {
        std::cerr << "GameLoop: core or view is null" << std::endl;
        return;
    }

    auto lastTime = std::chrono::high_resolution_clock::now();
    const double frameTime = 1.0 / 60.0;

    std::vector<int16_t> audioBuffer(AUDIO_BUFFER_SAMPLES);

    while (m_running.loadRelaxed()) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        if (m_input) {
            m_input->pollEvents();
        }

        m_core->runFrame();

        auto size = m_core->videoSize();
        if (size.width > 0 && size.height > 0) {
            m_view->updateFrame(m_core->videoBuffer(), size.width, size.height, m_core->pixelFormat());
        }

        if (m_audio) {
            size_t samples = m_core->readAudio(audioBuffer.data(), AUDIO_BUFFER_SAMPLES);
            if (samples > 0) {
                m_audio->enqueue(audioBuffer.data(), samples);
            }
        }

        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(frameEnd - frameStart).count();
        auto sleepTime = frameTime - elapsed;

        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }

        lastTime = frameStart;
    }
}

} // namespace OpenEmu
