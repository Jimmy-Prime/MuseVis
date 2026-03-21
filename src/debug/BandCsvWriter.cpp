#include "debug/BandCsvWriter.h"

#include <stdexcept>

namespace musevis {

BandCsvWriter::BandCsvWriter(std::string path)
    : out_(std::move(path), std::ios::out | std::ios::trunc) {
    if (!out_)
        throw std::runtime_error("Failed to open band dump CSV");

    out_ << "frame,analyzer";
    for (int i = 0; i < ANALYSIS_BANDS; ++i)
        out_ << ",band_" << i;
    out_ << '\n';
}

void BandCsvWriter::onFrame(const char* analyzerName,
                            uint64_t frameNumber,
                            const BandData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << frameNumber << ',' << analyzerName;
    for (float magnitude : data.magnitudes)
        out_ << ',' << magnitude;
    out_ << '\n';
}

} // namespace musevis
