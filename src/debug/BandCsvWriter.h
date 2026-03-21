#pragma once

#include <fstream>
#include <mutex>
#include <string>

#include "dsp/BandAnalyzer.h"

namespace musevis {

class BandCsvWriter final : public BandFrameObserver {
public:
    explicit BandCsvWriter(std::string path);
    ~BandCsvWriter() override = default;

    void onFrame(const char* analyzerName,
                 uint64_t frameNumber,
                 const BandData& data) override;

private:
    std::mutex mutex_;
    std::ofstream out_;
};

} // namespace musevis
