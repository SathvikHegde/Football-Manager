#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fm {

struct RngState {
    uint64_t state{0x853c49e6748fea9bULL};
    uint64_t inc{0xda3e39cb94b95bdbULL};

    constexpr RngState() = default;
    constexpr RngState(uint64_t init_state, uint64_t init_seq = 0xda3e39cb94b95bdbULL)
        : state(init_state), inc(init_seq) {}

    uint32_t next_u32() {
        uint64_t oldstate = state;
        state = oldstate * 6364136223846793005ULL + (inc | 1);
        uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = static_cast<uint32_t>(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    float next_float() { // Returns [0.0, 1.0)
        return static_cast<float>(next_u32()) / 4294967296.0f;
    }

    float next_gaussian(float mean, float stddev) {
        float u1 = std::max(1e-7f, next_float());
        float u2 = next_float();
        float z0 = std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530718f * u2);
        return mean + z0 * stddev;
    }

    [[nodiscard]] constexpr bool operator==(const RngState& o) const {
        return state == o.state && inc == o.inc;
    }
};

} // namespace fm

