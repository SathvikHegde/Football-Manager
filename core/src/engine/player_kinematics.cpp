#include "fm/engine/player_kinematics.hpp"
#include <cmath>
#include <algorithm>

namespace fm {

namespace PlayerKinematics {

float calculate_max_speed(const PlayerAttributes& attrs, float stamina_pct) {
    float pace_norm = attrs.norm(attrs.pace);
    return (6.0f + 3.2f * pace_norm) * (0.7f + 0.3f * stamina_pct);
}

float calculate_max_accel(const PlayerAttributes& attrs) {
    float accel_norm = attrs.norm(attrs.acceleration);
    return 2.5f + 2.5f * accel_norm; // m/s^2
}

void step(PlayerAgent& player, const Vec2& target_pos, float dt) {
    Vec2 to_target = target_pos - player.position;
    float dist = to_target.length();

    if (dist < 0.05f) {
        player.position = target_pos;
        player.velocity = Vec2{0.0f, 0.0f};
        return;
    }

    float max_speed = calculate_max_speed(player.attrs, player.stamina_pct);
    float max_accel = calculate_max_accel(player.attrs);

    Vec2 target_dir = to_target.normalized();
    Vec2 target_vel = target_dir * std::min(dist / dt, max_speed);

    Vec2 vel_diff = target_vel - player.velocity;
    float req_accel = vel_diff.length() / dt;

    Vec2 applied_accel = (req_accel > max_accel && req_accel > 0.0001f)
        ? vel_diff.normalized() * max_accel
        : vel_diff * (1.0f / dt);

    player.velocity = player.velocity + applied_accel * dt;
    player.position = player.position + player.velocity * dt;

    Vec2 facing_target_vec = (target_dir.length_sq() > 0.01f) ? target_dir : player.velocity;
    if (facing_target_vec.length_sq() > 0.01f) {
        float target_angle = std::atan2(facing_target_vec.y, facing_target_vec.x);
        float angle_diff = target_angle - player.facing_rad;
        constexpr float PI = 3.14159265358979323846f;
        constexpr float TWO_PI = 2.0f * PI;
        while (angle_diff > PI) angle_diff -= TWO_PI;
        while (angle_diff < -PI) angle_diff += TWO_PI;

        float max_turn = MAX_TURN_RATE * dt;
        player.facing_rad += std::clamp(angle_diff, -max_turn, max_turn);

        while (player.facing_rad > PI) player.facing_rad -= TWO_PI;
        while (player.facing_rad < -PI) player.facing_rad += TWO_PI;
    }
}

} // namespace PlayerKinematics
} // namespace fm

