#pragma once

#include <algorithm>
#include <cmath>

// h wraps around (any value), s and v are 0..1. Writes r, g, b in 0..1.
inline void hsvToRgb(float h, float s, float v, float* rgb) {
    h = h - std::floor(h);
    for (int i = 0; i < 3; ++i) {
        const float k = std::fmod(5.0f - 2.0f * static_cast<float>(i) + h * 6.0f, 6.0f);
        rgb[i] = v - v * s * std::clamp(std::min(k, 4.0f - k), 0.0f, 1.0f);
    }
}
