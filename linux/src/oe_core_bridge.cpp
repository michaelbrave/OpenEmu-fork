#include "oe_core_bridge.h"
#include <dlfcn.h>
#include <stdexcept>
#include <iostream>

namespace OpenEmu {

CoreBridge::CoreBridge(const std::string& libraryPath)
    : m_handle(nullptr), m_interface(nullptr), m_state(nullptr)
{
    m_handle = dlopen(libraryPath.c_str(), RTLD_LAZY);
    if (!m_handle) {
        throw std::runtime_error("dlopen failed: " + std::string(dlerror()));
    }

    auto get_interface = (const OECoreInterface* (*)())dlsym(m_handle, "oe_get_core_interface");
    if (!get_interface) {
        dlclose(m_handle);
        throw std::runtime_error("dlsym failed to find oe_get_core_interface");
    }

    m_interface = get_interface();
    if (!m_interface) {
        dlclose(m_handle);
        throw std::runtime_error("oe_get_core_interface returned NULL");
    }

    m_state = m_interface->create();
    if (!m_state) {
        dlclose(m_handle);
        throw std::runtime_error("core create failed");
    }
}

CoreBridge::~CoreBridge()
{
    if (m_state && m_interface) {
        m_interface->destroy(m_state);
    }
    if (m_handle) {
        dlclose(m_handle);
    }
}

bool CoreBridge::loadROM(const std::string& path)
{
    if (!m_state || !m_interface) return false;
    return m_interface->load_rom(m_state, path.c_str()) == 0;
}

void CoreBridge::runFrame()
{
    if (m_state && m_interface) {
        m_interface->run_frame(m_state);
    }
}

void CoreBridge::reset()
{
    if (m_state && m_interface) {
        m_interface->reset(m_state);
    }
}

bool CoreBridge::saveState(const std::string& path)
{
    if (!m_state || !m_interface) return false;
    return m_interface->save_state(m_state, path.c_str()) == 0;
}

bool CoreBridge::loadState(const std::string& path)
{
    if (!m_state || !m_interface) return false;
    return m_interface->load_state(m_state, path.c_str()) == 0;
}

void CoreBridge::setButton(int player, int button, bool pressed)
{
    if (m_state && m_interface) {
        m_interface->set_button(m_state, player, button, pressed ? 1 : 0);
    }
}

CoreBridge::VideoSize CoreBridge::videoSize() const
{
    int w = 0, h = 0;
    if (m_state && m_interface) {
        m_interface->get_video_size(m_state, &w, &h);
    }
    return {w, h};
}

OEPixelFormat CoreBridge::pixelFormat() const
{
    if (m_state && m_interface) {
        return m_interface->get_pixel_format(m_state);
    }
    return OE_PIXEL_RGBA8888;
}

const void* CoreBridge::videoBuffer() const
{
    if (m_state && m_interface) {
        return m_interface->get_video_buffer(m_state);
    }
    return nullptr;
}

int CoreBridge::audioSampleRate() const
{
    if (m_state && m_interface) {
        return m_interface->get_audio_sample_rate(m_state);
    }
    return 0;
}

size_t CoreBridge::readAudio(int16_t* buffer, size_t maxSamples)
{
    if (m_state && m_interface) {
        return m_interface->read_audio(m_state, buffer, maxSamples);
    }
    return 0;
}

} // namespace OpenEmu
