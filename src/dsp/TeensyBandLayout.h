#pragma once

#include <array>

#include "musevis/SharedState.h"

namespace musevis {

struct TeensyBandDefinition {
    float centerHz;
    float q;
    const char* label;
    float gain;
};

inline constexpr std::array<TeensyBandDefinition, NUM_BANDS> kTeensyBandLayout{{
    {   40.0f, 1.10f, " 40", 1.70f},
    {   63.0f, 1.10f, " 63", 1.55f},
    {  100.0f, 1.10f, "100", 1.35f},
    {  160.0f, 1.10f, "160", 1.15f},
    {  250.0f, 1.15f, "250", 1.00f},
    {  400.0f, 1.15f, "400", 0.95f},
    {  630.0f, 1.15f, "630", 0.92f},
    { 1000.0f, 1.20f, "1.0k", 0.92f},
    { 1600.0f, 1.20f, "1.6k", 0.95f},
    { 2500.0f, 1.25f, "2.5k", 1.00f},
    { 4000.0f, 1.25f, "4.0k", 1.05f},
    { 6300.0f, 1.30f, "6.3k", 1.12f},
    {10000.0f, 1.30f, " 10k", 1.18f},
    {16000.0f, 1.30f, " 16k", 1.25f},
}};

} // namespace musevis
