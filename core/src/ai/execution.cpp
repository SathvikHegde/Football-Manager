#include "fm/ai/execution.hpp"
#include "fm/engine/ball_physics.hpp"
#include "fm/engine/pitch.hpp"
#include <algorithm>
#include <cmath>

namespace fm {
namespace AI {

namespace {

float compute_pressure_factor(const MatchState& state, const PlayerAgent& carrier) {
    const int8_t opp_start = (carrier.team_id == 0) ? 11 : 0;
    float min_dist = 1e9f;
    for (int8_t o = opp_start; o < opp_start + 11; ++o) {
        const float d = (state.players[static_cast<std::size_t>(o)].position - carrier.position).length();
        if (d < min_dist) min_dist = d;
    }
    if (min_dist <= 2.5f) return 1.0f;
    if (min_dist >= 5.0f) return 0.0f;
    return (5.0f - min_dist) / 2.5f;
}

Vec2 rotate_vec2(const Vec2& v, float rad) {
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    return Vec2{v.x * c - v.y * s, v.x * s + v.y * c};
}

} // namespace

void execute_action(MatchState& state, uint8_t player_index) {
    std::vector<MatchEvent> unused;
    execute_action(state, player_index, unused);
}

void execute_action(MatchState& state, uint8_t player_index, std::vector<MatchEvent>& events) {
    if (player_index >= state.players.size()) return;
    if (state.ball.owner_slot != static_cast<int8_t>(player_index)) return;

    auto& player = state.players[player_index];
    const float attack_dir = Pitch::attack_direction(player.team_id, state.half);
    const float pressure = compute_pressure_factor(state, player);
    const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};

    if (player.current_action == ActionType::Pass) {
        if (player.action_target_slot < 0 || player.action_target_slot >= 22) {
            return;
        }

        const auto& receiver = state.players[static_cast<std::size_t>(player.action_target_slot)];
        const float raw_dist = (receiver.position - ball_2d).length();
        const float est_speed = (raw_dist <= 12.0f)
            ? std::clamp(raw_dist * 1.30f + 5.0f, 10.0f, 22.0f)
            : std::clamp(raw_dist * 0.80f + 9.0f, 16.0f, 28.0f);
        const float est_time = raw_dist / std::max(est_speed, 1.0f);
        Vec2 lead_target = receiver.position + receiver.velocity * (est_time * 0.55f);
        lead_target.x = std::clamp(lead_target.x, -Pitch::HALF_LENGTH + 2.0f, Pitch::HALF_LENGTH - 2.0f);
        lead_target.y = std::clamp(lead_target.y, -Pitch::HALF_WIDTH + 2.0f, Pitch::HALF_WIDTH - 2.0f);

        Vec2 ideal_dir = (lead_target - ball_2d).normalized();
        if (ideal_dir.length_sq() < 0.01f) {
            ideal_dir = Vec2{attack_dir, 0.0f};
        }

        const float attr_norm = player.attrs.norm(player.attrs.passing);
        const float sigma_theta = 0.15f * (1.0f - 0.7f * attr_norm) * (1.0f + 0.5f * pressure) * (2.0f - player.stamina_pct);
        const float theta_err = state.rng.next_gaussian(0.0f, sigma_theta);
        const float power_err = std::clamp(state.rng.next_gaussian(1.0f, 0.08f * (1.0f - 0.5f * attr_norm)), 0.80f, 1.20f);

        const Vec2 exec_dir = rotate_vec2(ideal_dir, theta_err);
        const float pass_speed = est_speed * power_err;
        const float vz = (raw_dist > 12.0f) ? std::clamp((raw_dist - 10.0f) * 0.22f, 1.0f, 5.5f) : 0.25f;

        state.ball.velocity = Vec3{exec_dir.x * pass_speed, exec_dir.y * pass_speed, vz};
        state.ball.owner_slot = -1;
        state.ball.last_touch_slot = static_cast<int8_t>(player_index);
        state.ball.last_touch_team = static_cast<int8_t>(player.team_id);
        state.ball.claim_lock_ticks = 4;
        state.shot_active = false;
        state.last_possessing_team = static_cast<int8_t>(player.team_id);
        state.possession_change_tick = state.tick;

        player.facing_rad = std::atan2(exec_dir.y, exec_dir.x);
        player.action_cooldown_ticks = 4;

        MatchEvent pass_evt{};
        pass_evt.tick = state.tick;
        pass_evt.type = EventType::Pass;
        pass_evt.primary_slot = static_cast<int8_t>(player_index);
        pass_evt.secondary_slot = player.action_target_slot;
        pass_evt.location = ball_2d;
        events.push_back(pass_evt);
        return;
    }

    if (player.current_action == ActionType::Shoot) {
        const float goal_x = Pitch::target_goal_x(player.team_id, state.half);
        const uint8_t opp_gk_slot = (player.team_id == 0) ? 11 : 0;
        const auto& opp_gk = state.players[opp_gk_slot];

        // Aim away from opponent GK inside the goal posts
        const float aim_y = (opp_gk.position.y >= 0.0f) ? -2.2f : 2.2f;
        const float aim_z = 0.45f + 1.15f * state.rng.next_float();
        const Vec2 aim_2d{goal_x, aim_y};

        Vec2 ideal_dir = (aim_2d - ball_2d).normalized();
        if (ideal_dir.length_sq() < 0.01f) {
            ideal_dir = Vec2{attack_dir, 0.0f};
        }

        const float attr_norm = player.attrs.norm(player.attrs.shooting);
        const float sigma_theta = 0.22f * (1.0f - 0.7f * attr_norm) * (1.0f + 0.5f * pressure) * (2.0f - player.stamina_pct);
        const float theta_err = state.rng.next_gaussian(0.0f, sigma_theta);
        const float power_err = std::clamp(state.rng.next_gaussian(1.0f, 0.08f * (1.0f - 0.5f * attr_norm)), 0.80f, 1.20f);

        const Vec2 exec_dir = rotate_vec2(ideal_dir, theta_err);
        const float shot_speed = (22.0f + 9.0f * attr_norm) * power_err;

        const float dist_to_goal = std::max(std::abs(goal_x - ball_2d.x), 1.0f);
        const float vx = exec_dir.x * shot_speed;
        const float vy = exec_dir.y * shot_speed;
        const float t_est = (std::abs(vx) > 1.0f) ? (dist_to_goal / std::abs(vx)) : (dist_to_goal / shot_speed);
        const float vz = std::clamp((aim_z - state.ball.position.z) / std::max(t_est, 0.1f) + 0.5f * 9.81f * t_est, 1.2f, 8.8f);

        state.ball.velocity = Vec3{vx, vy, vz};
        state.ball.owner_slot = -1;
        state.ball.last_touch_slot = static_cast<int8_t>(player_index);
        state.ball.last_touch_team = static_cast<int8_t>(player.team_id);
        state.ball.claim_lock_ticks = 4;
        state.last_possessing_team = static_cast<int8_t>(player.team_id);
        state.possession_change_tick = state.tick;

        // Compute predicted arrival point at the goal plane
        const float flight_t = (vx * attack_dir > 0.5f) ? ((goal_x - ball_2d.x) / vx) : 10.0f;
        const float arrival_y = ball_2d.y + vy * flight_t;
        const float arrival_z = std::max(0.0f, state.ball.position.z + vz * flight_t - 0.5f * 9.81f * flight_t * flight_t);

        state.shot_active = true;
        state.shot_shooter_slot = static_cast<int8_t>(player_index);
        state.shot_origin = ball_2d;
        state.shot_target_pos = Vec3{goal_x, arrival_y, arrival_z};
        state.shot_initial_speed = state.ball.velocity.length();

        player.facing_rad = std::atan2(exec_dir.y, exec_dir.x);
        player.action_cooldown_ticks = 5;

        const float dist_goal_norm = std::clamp(1.0f - (aim_2d - ball_2d).length() / 35.0f, 0.02f, 0.65f);
        const float xg_est = dist_goal_norm * (0.5f + 0.5f * attr_norm);

        MatchEvent shot_evt{};
        shot_evt.tick = state.tick;
        shot_evt.type = EventType::Shot;
        shot_evt.primary_slot = static_cast<int8_t>(player_index);
        shot_evt.location = ball_2d;
        shot_evt.xg_value = xg_est;
        events.push_back(shot_evt);

        if (std::abs(arrival_y) < Pitch::HALF_GOAL_WIDTH && arrival_z < Pitch::GOAL_HEIGHT) {
            MatchEvent sot_evt{};
            sot_evt.tick = state.tick;
            sot_evt.type = EventType::ShotOnTarget;
            sot_evt.primary_slot = static_cast<int8_t>(player_index);
            sot_evt.location = ball_2d;
            sot_evt.xg_value = xg_est;
            events.push_back(sot_evt);
        }
        return;
    }

    if (player.current_action == ActionType::Dribble) {
        player.move_target = player.action_target_pos;
    } else if (player.current_action == ActionType::Shield) {
        player.move_target = player.position;
    }
}

} // namespace AI
} // namespace fm
