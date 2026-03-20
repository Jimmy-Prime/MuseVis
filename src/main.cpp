#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "musevis/SharedState.h"
#include "audio/AudioCapture.h"
#include "led/LEDController.h"
#include "render/RenderEngine.h"

static std::atomic<bool> gRunning{true};

static void onSignal(int) {
    gRunning = false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: musevis <pulse-source>\n"
                  << "  e.g.: musevis musevis_sink.monitor\n";
        return 1;
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        musevis::SharedState  state;
        musevis::LEDController leds;
        musevis::RenderEngine  render(state, leds);
        musevis::AudioCapture  audio(state, argv[1]);

        render.start();
        audio.start();

        std::cout << "MuseVis running on " << argv[1]
                  << " — press Ctrl+C to quit.\n";

        while (gRunning)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        audio.stop();
        render.stop();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
