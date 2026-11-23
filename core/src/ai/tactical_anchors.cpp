#include "fm/ai/tactical_anchors.hpp"
#include "fm/engine/pitch.hpp"

namespace fm {
namespace AI {

Vec2 compute_dynamic_anchor(const TacticalConfig& tactics,
                            uint8_t player_slot,
                            const Vec2& ball_pos,
                            bool in_possession) {
    uint8_t norm_idx = player_slot % 11;
    const auto& norm = tactics.base_anchors_norm[norm_idx];

    // Base pitch anchor
    float anchor_x = (norm.x - 0.5f) * Pitch::LENGTH;
    float anchor_y = (norm.y - 0.5f) * Pitch::WIDTH;

    // Shift relative to ball position
    float shift_x = ball_pos.x * 0.35f * tactics.defensive_line;
    float shift_y = ball_pos.y * 0.25f;

    float final_x = anchor_x + shift_x;
    float final_y = anchor_y + shift_y;

    if (!in_possession) {
        // Compress team width by 15% out of possession
        final_y *= 0.85f;
    }

    return Vec2{final_x, final_y};
}

} // namespace AI
} // namespace fm

