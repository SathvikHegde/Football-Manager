#include "fm/ai/goalkeeper_ai.hpp"
#include "fm/engine/pitch.hpp"
#include <algorithm>
#include <cmath>

namespace fm {
namespace AI {

namespace {

float compute_save_margin(const MatchState& state, uint8_t gk_slot, const Vec3& shot_target, float shot_speed) {
    if (gk_slot >= state.players.size()) return -1.0f;
    if (shot_speed <= 0.1f) return -1.0f;
    if (std::abs(shot_target.y) > Pitch::HALF_GOAL_WIDTH || shot_target.z > Pitch::GOAL_HEIGHT || shot_target.z < 0.0f) {
        return -1.0f;
    }

    const auto& gk = state.players[gk_slot];
    const Vec2 arrival_pos{shot_target.x, shot_target.y};
    const Vec2 origin = (state.shot_active)
        ? state.shot_origin
        : Vec2{state.ball.position.x, state.ball.position.y};

    const float d_dive = std::hypot(
        arrival_pos.y - gk.position.y,
        std::max(0.0f, shot_target.z - 1.25f));
    const float d_shot = std::max((arrival_pos - origin).length(), 1.0f);
    const float t_flight = d_shot / shot_speed;
    const float t_react = 0.25f - 0.12f * gk.attrs.norm(gk.attrs.positioning);
    const float reach = (t_flight - t_react) * (4.5f + 4.0f * gk.attrs.norm(gk.attrs.acceleration));

    return reach - d_dive;
}

} // namespace

void update_goalkeeper_position(MatchState& state, uint8_t gk_slot) {
    if (gk_slot >= state.players.size()) return;
    auto& gk = state.players[gk_slot];
    const uint8_t team_id = gk.team_id;
    const float attack_dir = Pitch::attack_direction(team_id, state.half);
    const float own_goal_x = Pitch::own_goal_x(team_id, state.half);

    // If GK currently owns the ball, hold position for distribution
    if (state.ball.owner_slot == static_cast<int8_t>(gk_slot)) {
        gk.move_target = gk.position;
        return;
    }

    const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};

    // React to an incoming shot headed toward own goal
    if (state.shot_active && state.shot_shooter_slot >= 0) {
        const uint8_t shooter_team = state.players[static_cast<std::size_t>(state.shot_shooter_slot)].team_id;
        if (shooter_team != team_id) {
            const float toward_goal_vel = state.ball.velocity.x * (-attack_dir);
            if (toward_goal_vel > 2.0f) {
                const float target_y = std::clamp(state.shot_target_pos.y, -Pitch::HALF_GOAL_WIDTH, Pitch::HALF_GOAL_WIDTH);
                gk.move_target = Vec2{own_goal_x + attack_dir * 1.5f, target_y};
                gk.current_action = ActionType::Save;
                return;
            }
        }
    }

    // Loose ball inside own penalty box: GK sweeps if closest teammate
    if (state.ball.owner_slot == -1 &&
        Pitch::is_in_own_penalty_area(ball_2d, team_id, state.half)) {
        const float gk_dist_sq = (ball_2d - gk.position).length_sq();
        bool teammate_closer = false;
        const int8_t start = (team_id == 0) ? 1 : 12;
        const int8_t end = (team_id == 0) ? 10 : 21;
        for (int8_t i = start; i <= end; ++i) {
            const float d2 = (ball_2d - state.players[static_cast<std::size_t>(i)].position).length_sq();
            if (d2 + 1.0f < gk_dist_sq) {
                teammate_closer = true;
                break;
            }
        }
        if (!teammate_closer && gk_dist_sq < 256.0f) {
            gk.move_target = ball_2d;
            gk.current_action = ActionType::Intercept;
            return;
        }
    }

    // Angle bisector positioning between ball and two goalposts
    const Vec2 post1{own_goal_x, -Pitch::HALF_GOAL_WIDTH};
    const Vec2 post2{own_goal_x, Pitch::HALF_GOAL_WIDTH};
    const float d1 = (ball_2d - post1).length();
    const float d2 = (ball_2d - post2).length();
    const float y_bisect = (-Pitch::HALF_GOAL_WIDTH * d2 + Pitch::HALF_GOAL_WIDTH * d1) / std::max(d1 + d2, 0.01f);
    const Vec2 bisect_origin{own_goal_x, y_bisect};

    Vec2 dir_to_ball = (ball_2d - bisect_origin).normalized();
    if (dir_to_ball.length_sq() < 0.01f) {
        dir_to_ball = Vec2{attack_dir, 0.0f};
    }

    const float dist_ball_to_goal = (ball_2d - Vec2{own_goal_x, 0.0f}).length();
    const float t = std::clamp((dist_ball_to_goal - 16.5f) / (52.5f - 16.5f), 0.0f, 1.0f);
    const float depth = 1.5f + t * (8.0f - 1.5f);

    Vec2 target = bisect_origin + dir_to_ball * depth;
    target.x = (own_goal_x < 0.0f)
        ? std::clamp(target.x, own_goal_x + 0.8f, own_goal_x + 9.0f)
        : std::clamp(target.x, own_goal_x - 9.0f, own_goal_x - 0.8f);
    target.y = std::clamp(target.y, -Pitch::HALF_GOAL_WIDTH - 1.5f, Pitch::HALF_GOAL_WIDTH + 1.5f);

    gk.move_target = target;
    gk.current_action = ActionType::Move;
}

bool evaluate_shot_save(const MatchState& state, uint8_t gk_slot, const Vec3& shot_target, float shot_speed) {
    return compute_save_margin(state, gk_slot, shot_target, shot_speed) >= 0.0f;
}

bool resolve_shot_intercept(MatchState& state, uint8_t gk_slot, std::vector<MatchEvent>& events) {
    if (!state.shot_active || state.ball.owner_slot >= 0 || state.ball.claim_lock_ticks > 0) {
        return false;
    }
    if (gk_slot >= state.players.size()) return false;

    auto& gk = state.players[gk_slot];
    if (gk.action_cooldown_ticks > 0) return false;

    if (state.shot_shooter_slot >= 0 &&
        state.players[static_cast<std::size_t>(state.shot_shooter_slot)].team_id == gk.team_id) {
        return false;
    }

    const float attack_dir = Pitch::attack_direction(gk.team_id, state.half);
    const float own_goal_x = Pitch::own_goal_x(gk.team_id, state.half);

    // Ball must be moving toward own goal and near the goal-mouth intercept zone
    const float dist_from_goal_line = (state.ball.position.x - own_goal_x) * attack_dir;
    const float speed_toward_goal = state.ball.velocity.x * (-attack_dir);

    if (dist_from_goal_line < -0.2f || dist_from_goal_line > 3.5f || speed_toward_goal <= 1.0f) {
        return false;
    }

    if (state.ball.position.z > Pitch::GOAL_HEIGHT + 0.25f ||
        std::abs(state.ball.position.y) > Pitch::HALF_GOAL_WIDTH + 0.4f) {
        return false;
    }

    const float margin = compute_save_margin(state, gk_slot, state.shot_target_pos, state.shot_initial_speed);
    if (margin < 0.0f) {
        return false;
    }

    MatchEvent save_evt{};
    save_evt.tick = state.tick;
    save_evt.type = EventType::Save;
    save_evt.primary_slot = static_cast<int8_t>(gk_slot);
    save_evt.secondary_slot = state.shot_shooter_slot;
    save_evt.location = gk.position;
    events.push_back(save_evt);

    state.shot_active = false;
    gk.current_action = ActionType::Save;

    if (margin > 1.1f && state.ball.position.z < 2.1f) {
        // Comfortable catch
        state.ball.owner_slot = static_cast<int8_t>(gk_slot);
        state.ball.last_touch_slot = static_cast<int8_t>(gk_slot);
        state.ball.last_touch_team = static_cast<int8_t>(gk.team_id);
        state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
        state.ball.position.z = 0.0f;
        gk.action_cooldown_ticks = 6;
        state.last_possessing_team = static_cast<int8_t>(gk.team_id);
        state.possession_change_tick = state.tick;
    } else {
        // Parried deflection away from goal
        const float parry_sign_y = (state.shot_target_pos.y >= gk.position.y) ? 1.0f : -1.0f;
        const float vx = attack_dir * (6.5f + 4.0f * state.rng.next_float());
        const float vy = parry_sign_y * (6.0f + 5.0f * state.rng.next_float());
        state.ball.velocity = Vec3{vx, vy, 2.2f};
        state.ball.owner_slot = -1;
        state.ball.last_touch_slot = static_cast<int8_t>(gk_slot);
        state.ball.last_touch_team = static_cast<int8_t>(gk.team_id);
        state.ball.claim_lock_ticks = 6;
        gk.action_cooldown_ticks = 8;
        state.possession_change_tick = state.tick;
    }

    return true;
}

} // namespace AI
} // namespace fm
