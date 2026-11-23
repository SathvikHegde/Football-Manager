#include "fm/engine/match_simulator.hpp"
#include "fm/engine/ball_physics.hpp"
#include "fm/engine/player_kinematics.hpp"
#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"

namespace fm {

void MatchSimulator::reset(uint64_t /*seed*/) {
    recent_events_.clear();
    // Default reset for simulation state
}

void MatchSimulator::step_tick(MatchState& state) {
    recent_events_.clear();

    // Layer 1: Clock & Rules
    state.tick++;
    MatchRules::evaluate(state, recent_events_);

    // Layer 4: Ball Physics & Player Kinematics
    BallPhysics::step(state.ball, Pitch::DT);

    for (auto& player : state.players) {
        PlayerKinematics::step(player, player.move_target, Pitch::DT);
    }
}

} // namespace fm

