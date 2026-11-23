#pragma once
#include <algorithm>
#include <cstdint>

namespace fm {

struct PlayerAttributes {
    uint8_t pace{10};          // Movement top speed modifier
    uint8_t acceleration{10};  // Acceleration rate modifier
    uint8_t stamina{10};       // Fatigue drop-off rate
    uint8_t passing{10};       // Pass directional accuracy & variance
    uint8_t vision{10};        // Max effective pass evaluation distance & search cone angle
    uint8_t shooting{10};      // Shot accuracy, velocity control, xG execution
    uint8_t dribbling{10};     // Ball control tightness & speed while carrying
    uint8_t tackling{10};      // Tackle reach & success probability
    uint8_t positioning{10};   // Off-ball defensive anchor compliance
    uint8_t decisions{10};     // Utility AI noise reduction / optimal action choice rate

    [[nodiscard]] constexpr float norm(uint8_t val) const {
        return static_cast<float>(std::clamp<uint8_t>(val, 1, 20) - 1) / 19.0f;
    }
};

} // namespace fm

