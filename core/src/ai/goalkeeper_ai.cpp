#include "fm/ai/goalkeeper_ai.hpp"
#include "fm/engine/pitch.hpp"

namespace fm {
namespace AI {

void update_goalkeeper_position(MatchState& state, uint8_t gk_slot) {
    if (gk_slot >= state.players.size()) return;
    auto& gk = state.players[gk_slot];
    // Keep GK aligned with home or away goal line
    float goal_x = (gk_slot == 0) ? -Pitch::HALF_LENGTH + 2.0f : Pitch::HALF_LENGTH - 2.0f;
    gk.position.x = goal_x;
    gk.position.y = 0.0f;
}

bool evaluate_shot_save(const MatchState& /*state*/, uint8_t /*gk_slot*/, const Vec3& /*shot_target*/, float /*shot_speed*/) {
    return false; // Stub for Stage 0
}

} // namespace AI
} // namespace fm

