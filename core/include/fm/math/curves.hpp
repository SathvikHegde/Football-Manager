#pragma once
#include <algorithm>

namespace fm {

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float clamp(float v, float min_v, float max_v) {
    return std::clamp(v, min_v, max_v);
}

} // namespace fm

