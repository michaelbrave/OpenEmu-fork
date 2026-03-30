#include "oe_audio_backend.h"
#include <iostream>

namespace OpenEmu {

AudioBackend::AudioBackend()
    : m_device(0), m_sampleRate(0), m_channels(0)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_InitSubSystem(AUDIO) failed: " << SDL_GetError() << std::endl;
    }
}

AudioBackend::~AudioBackend()
{
    stop();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool AudioBackend::start(int sampleRate, int channels)
{
    stop();

    SDL_AudioSpec wanted, obtained;
    SDL_zero(wanted);
    wanted.freq = sampleRate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = channels;
    wanted.samples = 1024;
    wanted.callback = NULL; // We use SDL_QueueAudio

    m_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (m_device == 0) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_sampleRate = obtained.freq;
    m_channels = obtained.channels;

    SDL_PauseAudioDevice(m_device, 0);
    return true;
}

void AudioBackend::stop()
{
    if (m_device != 0) {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }
}

void AudioBackend::enqueue(const int16_t* samples, size_t count)
{
    if (m_device != 0) {
        SDL_QueueAudio(m_device, samples, count * sizeof(int16_t));
    }
}

} // namespace OpenEmu
