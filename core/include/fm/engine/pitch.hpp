#pragma once
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
}

} // namespace fm

