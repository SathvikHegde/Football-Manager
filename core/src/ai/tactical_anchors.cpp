#include "fm/ai/tactical_anchors.hpp"
#include "fm/engine/pitch.hpp"
#include <algorithm>
#include <cmath>

namespace fm {
namespace AI {

Vec2 compute_dynamic_anchor(const TacticalConfig& tactics,
                            uint8_t player_slot,
                            const Vec2& ball_pos,
                            bool in_possession,
                            float attack_dir,
                            RoleId role) {
    uint8_t norm_idx = player_slot % 11;
    const auto& norm = tactics.base_anchors_norm[norm_idx];

    // Base pitch anchor (mirrored by attack_dir so norm.x=0 is always own goal side)
    float anchor_x = (norm.x - 0.5f) * Pitch::LENGTH * attack_dir;
    float anchor_y = (norm.y - 0.5f) * Pitch::WIDTH * attack_dir;

    // Outfield defensive line shift
    float line_offset = (norm_idx == 0) ? 0.0f : ((tactics.defensive_line - 0.5f) * 14.0f * attack_dir);

    // Shift relative to ball position (defending teams compress vertically toward ball zone)
    float x_gain = in_possession
        ? (0.35f * tactics.defensive_line)
        : (0.28f + 0.24f * tactics.defensive_line);
    float shift_x = ball_pos.x * x_gain;
    float shift_y = ball_pos.y * 0.25f;

    float final_x = anchor_x + shift_x + line_offset;
    float final_y = anchor_y + shift_y;

    if (!in_possession) {
        // Compress team width by 15% out of possession
        final_y *= 0.85f;
        // Pull midfielders and attackers back into a compact defensive block when ball is deep in own half
        if (norm_idx != 0 && ball_pos.x * attack_dir < -10.0f) {
            const float deep_ratio = std::clamp((-10.0f - ball_pos.x * attack_dir) / 42.5f, 0.0f, 1.0f);
            final_x -= attack_dir * deep_ratio * 10.0f;
        }
    } else if (role == RoleId::Winger) {
        // In possession, wingers extend width toward touchlines (+/- 30.0m)
        float sign_y = (final_y >= 0.0f) ? 1.0f : -1.0f;
        final_y = sign_y * std::clamp(std::max(std::abs(final_y) * 1.15f, 28.0f), 26.0f, 30.5f);
    }

    final_x = std::clamp(final_x, -Pitch::HALF_LENGTH + 1.5f, Pitch::HALF_LENGTH - 1.5f);
    final_y = std::clamp(final_y, -Pitch::HALF_WIDTH + 1.5f, Pitch::HALF_WIDTH - 1.5f);

    return Vec2{final_x, final_y};
}

} // namespace AI
} // namespace fm
