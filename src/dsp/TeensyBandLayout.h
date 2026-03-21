#pragma once

#include <array>

#include "musevis/SharedState.h"

namespace musevis {

struct BandDefinition {
    float lowHz;
    float centerHz;
    float highHz;
    float gain;
};

inline constexpr std::array<BandDefinition, ANALYSIS_BANDS> kTeensyBandLayout{{
    {40.00f, 49.54f,   61.37f,   0.70f},
    {61.37f, 76.01f,   94.14f,   0.85f},
    {94.14f, 116.60f,  144.43f,  1.05f},
    {144.43f, 178.89f, 221.57f,  1.00f},
    {221.57f, 274.43f, 339.91f,  0.95f},
    {339.91f, 421.02f, 521.47f,  0.95f},
    {521.47f, 645.89f, 800.00f,  0.95f},
    {800.00f, 990.88f, 1227.30f, 0.95f},
    {1227.30f, 1520.13f, 1882.84f, 0.98f},
    {1882.84f, 2332.08f, 2888.51f, 1.00f},
    {2888.51f, 3577.71f, 4431.35f, 1.04f},
    {4431.35f, 5488.66f, 6798.25f, 1.08f},
    {6798.25f, 8420.31f, 10429.38f, 1.12f},
    {10429.38f, 12917.82f, 16000.00f, 1.18f},
}};

} // namespace musevis
