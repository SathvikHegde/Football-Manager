#pragma once
#include <cstdint>
#include <cmath>
#include "fm/math/vec2.hpp"

namespace fm {

namespace Pitch {
    constexpr float LENGTH = 105.0f;
    constexpr float WIDTH = 68.0f;
    constexpr float HALF_LENGTH = 52.5f;
    constexpr float HALF_WIDTH = 34.0f;
    constexpr float GOAL_WIDTH = 7.32f;
    constexpr float HALF_GOAL_WIDTH = 3.66f;
    constexpr float GOAL_HEIGHT = 2.44f;
    constexpr float PENALTY_AREA_LENGTH = 16.5f;
    constexpr float PENALTY_AREA_WIDTH = 40.32f;
    constexpr float HALF_PENALTY_AREA_WIDTH = 20.16f;
    constexpr float CENTER_CIRCLE_RADIUS = 9.15f;
    constexpr float DT = 0.05f; // 20 Hz
    constexpr uint32_t TICKS_PER_HALF = 54000;
    constexpr uint32_t TICKS_FULL_TIME = 108000;

    [[nodiscard]] constexpr bool is_in_bounds(const Vec2& pos) {
        return pos.x >= -HALF_LENGTH && pos.x <= HALF_LENGTH &&
               pos.y >= -HALF_WIDTH && pos.y <= HALF_WIDTH;
    }

    // Returns +1.0f if attacking +X, -1.0f if attacking -X.
    // Home (team_id 0) attacks +X in half 1 and -X in half 2.
    [[nodiscard]] constexpr float attack_direction(uint8_t team_id, uint8_t half) {
        bool attacks_pos_x = (team_id == 0) ? (half == 1) : (half != 1);
        return attacks_pos_x ? 1.0f : -1.0f;
    }

    [[nodiscard]] constexpr float own_goal_x(uint8_t team_id, uint8_t half) {
        return -attack_direction(team_id, half) * HALF_LENGTH;
    }

    [[nodiscard]] constexpr float target_goal_x(uint8_t team_id, uint8_t half) {
        return attack_direction(team_id, half) * HALF_LENGTH;
    }

    [[nodiscard]] inline bool is_in_own_penalty_area(const Vec2& pos, uint8_t team_id, uint8_t half) {
        float gx = own_goal_x(team_id, half);
        float dist_from_line = (gx < 0.0f) ? (pos.x - gx) : (gx - pos.x);
        return dist_from_line >= 0.0f && dist_from_line <= PENALTY_AREA_LENGTH &&
               std::abs(pos.y) <= HALF_PENALTY_AREA_WIDTH;
    }
}

} // namespace fm
