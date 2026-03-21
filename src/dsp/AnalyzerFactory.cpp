#include "dsp/AnalyzerFactory.h"

#include <cstdlib>
#include <memory>
#include <string_view>

#include "debug/BandCsvWriter.h"
#include "dsp/FFTProcessor.h"
#include "dsp/FilterBankProcessor.h"

namespace musevis {

namespace {

std::string_view envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string_view(value) : std::string_view{};
}

} // namespace

AnalyzerKind analyzerKindFromEnvironment() {
    const auto value = envOrEmpty("MUSEVIS_ANALYZER");
    if (value == "fft")
        return AnalyzerKind::FFT;
    return AnalyzerKind::FilterBank;
}

const char* analyzerKindName(AnalyzerKind kind) {
    switch (kind) {
        case AnalyzerKind::FFT:
            return "fft";
        case AnalyzerKind::FilterBank:
        default:
            return "filterbank";
    }
}

std::unique_ptr<BandFrameObserver> makeBandObserverFromEnvironment() {
    const auto path = envOrEmpty("MUSEVIS_BAND_DUMP");
    if (path.empty())
        return nullptr;
    return std::make_unique<BandCsvWriter>(std::string(path));
}

std::unique_ptr<BandAnalyzer> makeAnalyzer(AnalyzerKind kind,
                                           SharedState& state,
                                           BandFrameObserver* observer) {
    switch (kind) {
        case AnalyzerKind::FFT:
            return std::make_unique<FFTProcessor>(state, observer);
        case AnalyzerKind::FilterBank:
        default:
            return std::make_unique<FilterBankProcessor>(state, observer);
    }
}

} // namespace musevis
