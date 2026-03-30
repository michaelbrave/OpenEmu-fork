#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "oe_core_bridge.h"
#include "oe_audio_backend.h"
#include "oe_input_backend.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 3) {
        std::cerr << "Usage: audio_input_test <core_so> <rom_path>" << std::endl;
        return 1;
    }

    std::string corePath = argv[1];
    std::string romPath = argv[2];

    try {
        OpenEmu::CoreBridge core(corePath);
        if (!core.loadROM(romPath)) {
            std::cerr << "Failed to load ROM" << std::endl;
            return 1;
        }

        OpenEmu::AudioBackend audio;
        int rate = core.audioSampleRate();
        if (rate == 0) rate = 44100;
        if (!audio.start(rate, 2)) {
            std::cerr << "Failed to start audio" << std::endl;
            return 1;
        }

        OpenEmu::InputBackend input;
        input.setButtonCallback([&core](int player, int button, OpenEmu::InputEvent event) {
            core.setButton(player, button, event == OpenEmu::InputEvent::Press);
            std::cout << "Button " << button << (event == OpenEmu::InputEvent::Press ? " PRESSED" : " RELEASED") << std::endl;
        });

        std::cout << "Emulation started. Running for 5 seconds. Press keys to test input..." << std::endl;

        QTimer timer;
        int frames = 0;
        QObject::connect(&timer, &QTimer::timeout, [&]() {
            input.pollEvents();
            core.runFrame();
            
            int16_t samples[4096];
            size_t count = core.readAudio(samples, 4096);
            if (count > 0) {
                audio.enqueue(samples, count);
            }

            frames++;
            if (frames >= 300) { // ~5 seconds at 60fps
                std::cout << "Test finished." << std::endl;
                app.quit();
            }
        });
        timer.start(16); // ~60fps

        return app.exec();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
