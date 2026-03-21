#include "dsp/AnalyzerFactory.h"

#include <memory>

#include "dsp/FilterBankProcessor.h"

namespace musevis {

std::unique_ptr<BandAnalyzer> createDefaultAnalyzer(SharedState& state) {
    return std::make_unique<FilterBankProcessor>(state);
}

} // namespace musevis
