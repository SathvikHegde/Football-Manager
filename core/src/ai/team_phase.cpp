#include "fm/ai/team_phase.hpp"

namespace fm {
namespace AI {

TeamPhase classify_phase(const MatchState& state, uint8_t team_id) {
    if (state.ball.owner_slot >= 0) {
        uint8_t owner_team = (state.ball.owner_slot < 11) ? 0 : 1;
        return (owner_team == team_id) ? TeamPhase::InPossession : TeamPhase::OutOfPossession;
    }

    // Ball is loose / in flight
    const int8_t tid = static_cast<int8_t>(team_id);
    const uint32_t ticks_since_change = (state.tick >= state.possession_change_tick)
        ? (state.tick - state.possession_change_tick)
        : 9999u;

    // Within 3 seconds (60 ticks) of a turnover / possession disruption
    if (ticks_since_change <= 60u && state.last_possessing_team >= 0) {
        if (state.ball.last_touch_team == tid && state.last_possessing_team == tid) {
            return TeamPhase::InPossession;
        }
        if (state.ball.last_touch_team != tid && state.last_possessing_team != tid && state.ball.last_touch_team >= 0) {
            return TeamPhase::OutOfPossession;
        }
        return (state.ball.last_touch_team == tid)
            ? TeamPhase::AttackingTransition
            : TeamPhase::DefensiveTransition;
    }

    if (state.ball.last_touch_team == tid) {
        return TeamPhase::InPossession;
    }
    return TeamPhase::OutOfPossession;
}

} // namespace AI
} // namespace fm
