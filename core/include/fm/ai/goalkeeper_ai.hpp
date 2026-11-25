#pragma once
#include <vector>
#include "fm/model/match_event.hpp"
#include "fm/model/match_state.hpp"

namespace fm {

namespace AI {

void update_goalkeeper_position(MatchState& state, uint8_t gk_slot);
bool evaluate_shot_save(const MatchState& state, uint8_t gk_slot, const Vec3& shot_target, float shot_speed);
bool resolve_shot_intercept(MatchState& state, uint8_t gk_slot, std::vector<MatchEvent>& events);

} // namespace AI
} // namespace fm
