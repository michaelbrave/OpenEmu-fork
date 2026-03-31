#ifndef OE_CORE_BRIDGE_H
#define OE_CORE_BRIDGE_H

#include "oe_core_interface.h"
#include <string>
#include <vector>
#include <memory>

namespace OpenEmu {

class CoreBridgeImpl;

class CoreBridge {
public:
    CoreBridge(const std::string& libraryPath);
    ~CoreBridge();

    bool loadROM(const std::string& path);
    void setSaveDirectory(const std::string& path);
    void runFrame();
    void reset();

    bool saveState(const std::string& path);
    bool loadState(const std::string& path);

    void setButton(int player, int button, bool pressed);

    struct VideoSize {
        int width;
        int height;
    };
    VideoSize videoSize() const;
    OEPixelFormat pixelFormat() const;
    const void* videoBuffer() const;

    int audioSampleRate() const;
    size_t readAudio(int16_t* buffer, size_t maxSamples);
    bool isRunning() const;

private:
    std::shared_ptr<CoreBridgeImpl> m_impl;
};

} // namespace OpenEmu

#endif // OE_CORE_BRIDGE_H
