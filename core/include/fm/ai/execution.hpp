#pragma once
#include <vector>
#include "fm/model/match_event.hpp"
#include "fm/model/match_state.hpp"

namespace fm {

namespace AI {

void execute_action(MatchState& state, uint8_t player_index);
void execute_action(MatchState& state, uint8_t player_index, std::vector<MatchEvent>& events);

} // namespace AI
} // namespace fm
