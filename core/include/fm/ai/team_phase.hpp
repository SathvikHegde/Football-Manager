#pragma once
#include "fm/model/match_state.hpp"

namespace fm {

namespace AI {

enum class TeamPhase : uint8_t {
    InPossession,
    OutOfPossession,
    AttackingTransition,
    DefensiveTransition
};

TeamPhase classify_phase(const MatchState& state, uint8_t team_id);

} // namespace AI
} // namespace fm

