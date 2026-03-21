#pragma once

#include <memory>

namespace musevis {

class BandAnalyzer;
struct SharedState;

std::unique_ptr<BandAnalyzer> createDefaultAnalyzer(SharedState& state);

} // namespace musevis
