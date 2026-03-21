#pragma once

#include <memory>

#include "dsp/BandAnalyzer.h"

namespace musevis {

enum class AnalyzerKind {
    FilterBank,
    FFT,
};

AnalyzerKind analyzerKindFromEnvironment();
const char* analyzerKindName(AnalyzerKind kind);

std::unique_ptr<BandFrameObserver> makeBandObserverFromEnvironment();
std::unique_ptr<BandAnalyzer> makeAnalyzer(AnalyzerKind kind,
                                           SharedState& state,
                                           BandFrameObserver* observer = nullptr);

} // namespace musevis
