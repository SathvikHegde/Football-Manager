#pragma once
#include "fm/model/player.hpp"

namespace fm {

namespace PlayerKinematics {
    constexpr float MAX_TURN_RATE = 8.0f; // rad/s

    float calculate_max_speed(const PlayerAttributes& attrs, float stamina_pct);
    float calculate_max_accel(const PlayerAttributes& attrs);
    void step(PlayerAgent& player, const Vec2& target_pos, float dt = 0.05f);
}

} // namespace fm

