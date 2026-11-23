#include "fm/ai/team_phase.hpp"

namespace fm {
namespace AI {

TeamPhase classify_phase(const MatchState& state, uint8_t team_id) {
    if (state.ball.owner_slot >= 0) {
        uint8_t owner_team = (state.ball.owner_slot < 11) ? 0 : 1;
        return (owner_team == team_id) ? TeamPhase::InPossession : TeamPhase::OutOfPossession;
    }
    return TeamPhase::AttackingTransition;
}

} // namespace AI
} // namespace fm

