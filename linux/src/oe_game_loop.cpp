#include "oe_game_loop.h"
#include <QMetaObject>
#include <QPointer>
#include <chrono>
#include <thread>
#include <iostream>

namespace OpenEmu {

static const int AUDIO_BUFFER_SAMPLES = 2048;

static size_t bytesPerPixel(OEPixelFormat format)
{
    switch (format) {
        case OE_PIXEL_RGBA8888: return 4;
        case OE_PIXEL_RGB565:   return 2;
        case OE_PIXEL_INDEXED8: return 1;
    }
    return 0;
}

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

    const double frameTime = 1.0 / 60.0;

    std::vector<int16_t> audioBuffer(AUDIO_BUFFER_SAMPLES);
    QPointer<EmulatorView> view(m_view);

    while (m_running.loadRelaxed()) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        if (m_input) {
            m_input->pollEvents();
        }

        m_core->runFrame();

        auto size = m_core->videoSize();
        if (size.width > 0 && size.height > 0) {
            const OEPixelFormat format = m_core->pixelFormat();
            const void* buffer = m_core->videoBuffer();
            const size_t frameBytes = static_cast<size_t>(size.width) *
                                      static_cast<size_t>(size.height) *
                                      bytesPerPixel(format);
            if (buffer && frameBytes > 0 && view) {
                std::vector<uint8_t> frameCopy(frameBytes);
                memcpy(frameCopy.data(), buffer, frameBytes);
                QMetaObject::invokeMethod(
                    view.data(),
                    [view, frame = std::move(frameCopy), width = size.width, height = size.height, format]() mutable {
                        if (view) {
                            view->updateFrame(frame.data(), width, height, format);
                        }
                    },
                    Qt::QueuedConnection);
            }
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
    }
}

} // namespace OpenEmu
