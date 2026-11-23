#pragma once
#include "fm/math/vec2.hpp"
#include "fm/model/match_state.hpp"

namespace fm {

namespace AI {

Vec2 compute_dynamic_anchor(const TacticalConfig& tactics,
                            uint8_t player_slot,
                            const Vec2& ball_pos,
                            bool in_possession);

} // namespace AI
} // namespace fm

