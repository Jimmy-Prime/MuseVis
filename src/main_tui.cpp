#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "musevis/SharedState.h"
#include "audio/AudioCapture.h"
#include "render/TerminalRenderer.h"

static std::atomic<bool> gRunning{true};

static void onSignal(int) {
    gRunning = false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: musevis-tui <pulse-source>\n"
                  << "  e.g.: musevis-tui musevis_sink.monitor\n";
        return 1;
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        musevis::SharedState       state;
        musevis::TerminalRenderer  renderer(state);
        musevis::AudioCapture      audio(state, argv[1]);

        renderer.start();
        audio.start();

        while (gRunning)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        audio.stop();
        renderer.stop();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
