#include "audio/AudioCapture.h"

#include <pulse/error.h>
#include <pulse/simple.h>
#include <vector>

namespace musevis {

void AudioCapture::captureLoop() {
    pa_sample_spec ss{};
    ss.format   = PA_SAMPLE_FLOAT32LE;
    ss.rate     = static_cast<uint32_t>(SAMPLE_RATE);
    ss.channels = static_cast<uint8_t>(NUM_CHANNELS);

    int error = 0;
    pa_simple* pa = pa_simple_new(
        nullptr,              // default server
        "MuseVis",            // application name
        PA_STREAM_RECORD,
        sourceName_.c_str(),  // source device (e.g. musevis_sink.monitor)
        "audio capture",      // stream description
        &ss,
        nullptr,              // default channel map
        nullptr,              // default buffer attributes
        &error
    );

    if (!pa)
        throw std::runtime_error(std::string("pa_simple_new failed: ") + pa_strerror(error));

    // CHUNK_SIZE stereo frames × 2 channels × sizeof(float)
    constexpr int bufBytes = CHUNK_SIZE * NUM_CHANNELS * sizeof(float);
    std::vector<float> buf(CHUNK_SIZE * NUM_CHANNELS);

    while (running_) {
        if (pa_simple_read(pa, buf.data(), bufBytes, &error) < 0)
            break;  // device closed or error

        processCapturedFrames(buf.data(), CHUNK_SIZE);
    }

    pa_simple_free(pa);
}

} // namespace musevis
