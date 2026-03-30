#ifndef OE_AUDIO_BACKEND_H
#define OE_AUDIO_BACKEND_H

#include <SDL2/SDL.h>
#include <vector>
#include <stdint.h>

namespace OpenEmu {

class AudioBackend {
public:
    AudioBackend();
    ~AudioBackend();

    bool start(int sampleRate, int channels);
    void stop();

    void enqueue(const int16_t* samples, size_t count);

private:
    SDL_AudioDeviceID m_device;
    int m_sampleRate;
    int m_channels;
};

} // namespace OpenEmu

#endif // OE_AUDIO_BACKEND_H
