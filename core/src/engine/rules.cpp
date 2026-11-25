#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/ai/tactical_anchors.hpp"
#include <algorithm>
#include <cmath>

namespace fm {

namespace {

int8_t select_nearest_outfield_taker(const MatchState& state, uint8_t team_id, const Vec2& spot) {
    int8_t start = (team_id == 0) ? 1 : 12;
    int8_t end = (team_id == 0) ? 10 : 21;
    int8_t best_slot = start;
    float best_dist_sq = 1e9f;

    for (int8_t i = start; i <= end; ++i) {
        float d2 = (state.players[static_cast<std::size_t>(i)].position - spot).length_sq();
        if (d2 < best_dist_sq) {
            best_dist_sq = d2;
            best_slot = i;
        }
    }
    return best_slot;
}

} // namespace

void MatchRules::setup_kickoff(MatchState& state, uint8_t kicking_team) {
    state.play_state = PlayState::Kickoff;
    state.kickoff_team = kicking_team;
    state.restart_team = static_cast<int8_t>(kicking_team);
    state.restart_spot = Vec2{0.0f, 0.0f};
    state.restart_timer_ticks = 12; // 0.6s setup pause

    state.ball.position = Vec3{0.0f, 0.0f, 0.0f};
    state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
    state.ball.spin_rad_s = Vec3{0.0f, 0.0f, 0.0f};
    state.ball.owner_slot = -1;
    state.ball.claim_lock_ticks = 0;
    state.shot_active = false;
    state.stuck_ball_ticks = 0;

    int8_t taker_slot = (kicking_team == 0) ? 9 : 20;
    state.restart_taker_slot = taker_slot;

    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state.players[i];
        const auto& tactics = (p.team_id == 0) ? state.home_tactics : state.away_tactics;
        float attack_dir = Pitch::attack_direction(p.team_id, state.half);

        Vec2 anchor = AI::compute_dynamic_anchor(
            tactics, p.slot_index, Vec2{0.0f, 0.0f}, p.team_id == kicking_team, attack_dir, p.role);

        // Keep every player in their own half
        if (anchor.x * attack_dir > -2.5f) {
            anchor.x = -2.5f * attack_dir;
        }
        // Keep players outside the 9.15m center circle
        if (anchor.length() < Pitch::CENTER_CIRCLE_RADIUS + 0.6f) {
            anchor.x = -10.2f * attack_dir;
        }

        p.position = anchor;
        p.velocity = Vec2{0.0f, 0.0f};
        p.move_target = anchor;
        p.facing_rad = (attack_dir > 0.0f) ? 0.0f : 3.14159265f;
        p.current_action = ActionType::Idle;
        p.action_cooldown_ticks = 0;
        p.action_target_slot = -1;
    }

    // Place kickoff taker beside center spot on own side
    auto& taker = state.players[static_cast<std::size_t>(taker_slot)];
    float taker_dir = Pitch::attack_direction(taker.team_id, state.half);
    taker.position = Vec2{-taker_dir * 0.45f, 0.0f};
    taker.move_target = taker.position;
    taker.facing_rad = (taker_dir > 0.0f) ? 3.14159265f : 0.0f; // Faces own midfield for safe kickoff pass
}

void MatchRules::execute_restart(MatchState& state, std::vector<MatchEvent>& events) {
    const PlayState prev_state = state.play_state;
    const uint8_t restart_team = (state.restart_team == 1) ? 1 : 0;
    const float attack_dir = Pitch::attack_direction(restart_team, state.half);

    int8_t taker_slot = state.restart_taker_slot;
    const int8_t min_slot = (restart_team == 0) ? 0 : 11;
    const int8_t max_slot = (restart_team == 0) ? 10 : 21;
    if (taker_slot < min_slot || taker_slot > max_slot) {
        taker_slot = select_nearest_outfield_taker(state, restart_team, state.restart_spot);
    }

    auto& taker = state.players[static_cast<std::size_t>(taker_slot)];
    const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};

    // Snap taker into kicking range if still arriving
    if ((taker.position - ball_2d).length() > 1.2f) {
        Vec2 offset = (Vec2{0.0f, 0.0f} - ball_2d).normalized() * 0.45f;
        taker.position = ball_2d - offset;
        taker.velocity = Vec2{0.0f, 0.0f};
    }

    // Select best open teammate inside safe pitch interior
    int8_t best_target = -1;
    float best_score = -1e9f;

    for (int8_t i = min_slot; i <= max_slot; ++i) {
        if (i == taker_slot) continue;
        const auto& mate = state.players[static_cast<std::size_t>(i)];

        // Avoid passing back to GK on throw-in / corner / goal kick
        if (prev_state != PlayState::Kickoff && mate.role == RoleId::Goalkeeper) continue;

        float d = (mate.position - ball_2d).length();
        if (d < 4.0f || d > 45.0f) continue;

        // Check nearest opponent to candidate receiver
        float min_opp = 1e9f;
        int8_t opp_start = (restart_team == 0) ? 11 : 0;
        for (int8_t o = opp_start; o < opp_start + 11; ++o) {
            float od = (state.players[static_cast<std::size_t>(o)].position - mate.position).length();
            if (od < min_opp) min_opp = od;
        }

        float score = min_opp * 2.0f - std::abs(d - 16.0f) * 0.3f;
        if (prev_state == PlayState::Kickoff) {
            // Prefer midfield/defender in own half for clean kickoff circulation
            if (mate.position.x * attack_dir < 0.0f) score += 20.0f;
            if (mate.role == RoleId::Midfielder) score += 8.0f;
        } else if (prev_state == PlayState::Corner) {
            // Aim toward attacking box for corners
            if (mate.position.x * attack_dir > 30.0f && std::abs(mate.position.y) < 16.0f) {
                score += 18.0f;
            }
        } else {
            score += (mate.position.x - ball_2d.x) * attack_dir * 0.25f;
        }

        if (score > best_score) {
            best_score = score;
            best_target = i;
        }
    }

    Vec2 target_pos{};
    if (best_target >= 0) {
        target_pos = state.players[static_cast<std::size_t>(best_target)].position;
    } else {
        target_pos = Vec2{ball_2d.x + attack_dir * 18.0f, -ball_2d.y * 0.5f};
    }

    // Clamp target strictly inside pitch boundaries so restart never loops immediately out
    target_pos.x = std::clamp(target_pos.x, -Pitch::HALF_LENGTH + 3.0f, Pitch::HALF_LENGTH - 3.0f);
    target_pos.y = std::clamp(target_pos.y, -Pitch::HALF_WIDTH + 3.0f, Pitch::HALF_WIDTH - 3.0f);

    Vec2 dir = (target_pos - ball_2d).normalized();
    if (dir.length_sq() < 0.01f) {
        dir = Vec2{attack_dir, 0.0f};
    }

    // Modest Gaussian angular variance
    float err_rad = state.rng.next_gaussian(0.0f, 0.04f);
    float c = std::cos(err_rad);
    float s = std::sin(err_rad);
    Vec2 noisy_dir{dir.x * c - dir.y * s, dir.x * s + dir.y * c};

    float dist = (target_pos - ball_2d).length();
    float speed = std::clamp(11.0f + dist * 0.42f, 11.0f, 24.0f);
    float vz = 0.0f;
    if (prev_state == PlayState::ThrowIn) {
        vz = 1.6f;
    } else if (prev_state == PlayState::GoalKick) {
        speed = std::clamp(16.0f + dist * 0.35f, 16.0f, 26.0f);
        vz = 3.2f;
    } else if (prev_state == PlayState::Corner) {
        speed = std::clamp(14.0f + dist * 0.35f, 14.0f, 24.0f);
        vz = 3.6f;
    }

    // Ensure ball starts just inside the field lines on restart
    state.ball.position.x = std::clamp(state.ball.position.x, -Pitch::HALF_LENGTH + 0.25f, Pitch::HALF_LENGTH - 0.25f);
    state.ball.position.y = std::clamp(state.ball.position.y, -Pitch::HALF_WIDTH + 0.25f, Pitch::HALF_WIDTH - 0.25f);
    state.ball.position.z = (prev_state == PlayState::ThrowIn) ? 1.4f : 0.0f;
    state.ball.velocity = Vec3{noisy_dir.x * speed, noisy_dir.y * speed, vz};
    state.ball.owner_slot = -1;
    state.ball.last_touch_slot = taker_slot;
    state.ball.last_touch_team = static_cast<int8_t>(restart_team);
    state.ball.claim_lock_ticks = 4;

    taker.facing_rad = std::atan2(noisy_dir.y, noisy_dir.x);
    taker.current_action = ActionType::Pass;
    taker.action_cooldown_ticks = 4;

    state.last_possessing_team = static_cast<int8_t>(restart_team);
    state.possession_change_tick = state.tick;
    state.shot_active = false;
    state.stuck_ball_ticks = 0;

    if (prev_state == PlayState::Kickoff) {
        MatchEvent ko_evt{};
        ko_evt.tick = state.tick;
        ko_evt.type = EventType::Kickoff;
        ko_evt.primary_slot = taker_slot;
        ko_evt.location = ball_2d;
        events.push_back(ko_evt);
    }

    MatchEvent pass_evt{};
    pass_evt.tick = state.tick;
    pass_evt.type = EventType::Pass;
    pass_evt.primary_slot = taker_slot;
    pass_evt.secondary_slot = best_target;
    pass_evt.location = ball_2d;
    events.push_back(pass_evt);

    state.play_state = PlayState::OpenPlay;
    state.restart_taker_slot = -1;
}

void MatchRules::evaluate(MatchState& state, std::vector<MatchEvent>& events) {
    // 1. Half-time and Full-time clock boundaries
    if (state.tick == Pitch::TICKS_PER_HALF && state.half == 1) {
        state.play_state = PlayState::HalfTime;
        state.half = 2;
        state.kickoff_team = 1; // Away team kicks off 2nd half
        state.restart_timer_ticks = 40;
        state.ball.owner_slot = -1;
        state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
        state.shot_active = false;

        MatchEvent evt{};
        evt.tick = state.tick;
        evt.type = EventType::HalfTime;
        events.push_back(evt);
        return;
    }

    if (state.tick >= Pitch::TICKS_FULL_TIME) {
        if (state.play_state != PlayState::FullTime) {
            state.play_state = PlayState::FullTime;
            state.ball.owner_slot = -1;
            state.shot_active = false;

            MatchEvent evt{};
            evt.tick = state.tick;
            evt.type = EventType::FullTime;
            events.push_back(evt);
        }
        return;
    }

    // 2. Non-open-play waiting states (GoalScored, HalfTime)
    if (state.play_state == PlayState::GoalScored) {
        if (state.restart_timer_ticks > 0) {
            state.restart_timer_ticks--;
        }
        if (state.restart_timer_ticks == 0) {
            setup_kickoff(state, state.kickoff_team);
        }
        return;
    }

    if (state.play_state == PlayState::HalfTime) {
        if (state.restart_timer_ticks > 0) {
            state.restart_timer_ticks--;
        }
        if (state.restart_timer_ticks == 0) {
            for (auto& p : state.players) {
                p.stamina_pct = std::min(1.0f, p.stamina_pct + 0.10f);
            }
            setup_kickoff(state, state.kickoff_team);
        }
        return;
    }

    const auto& ball = state.ball;

    // 3. Goal detection (checked in OpenPlay and Kickoff for unit test compatibility)
    if ((state.play_state == PlayState::OpenPlay || state.play_state == PlayState::Kickoff) &&
        std::abs(ball.position.x) > Pitch::HALF_LENGTH &&
        std::abs(ball.position.y) < Pitch::HALF_GOAL_WIDTH &&
        ball.position.z < Pitch::GOAL_HEIGHT) {

        const bool hit_pos_x_goal = (ball.position.x > 0.0f);
        const bool home_scored = (state.half == 1) ? hit_pos_x_goal : !hit_pos_x_goal;

        MatchEvent goal_evt{};
        goal_evt.tick = state.tick;
        goal_evt.type = EventType::Goal;
        goal_evt.primary_slot = ball.last_touch_slot;
        goal_evt.location = Vec2{ball.position.x, ball.position.y};

        if (home_scored) {
            state.score_home++;
            state.kickoff_team = 1; // Conceding away team kicks off
        } else {
            state.score_away++;
            state.kickoff_team = 0; // Conceding home team kicks off
        }

        events.push_back(goal_evt);
        state.play_state = PlayState::GoalScored;
        state.restart_timer_ticks = 60; // 3 seconds celebration delay
        state.ball.owner_slot = -1;
        state.shot_active = false;
        state.ball.position.x = std::clamp(state.ball.position.x, -Pitch::HALF_LENGTH - 1.8f, Pitch::HALF_LENGTH + 1.8f);
        state.ball.position.y = std::clamp(state.ball.position.y, -Pitch::HALF_GOAL_WIDTH + 0.3f, Pitch::HALF_GOAL_WIDTH - 0.3f);
        state.ball.velocity = state.ball.velocity * 0.25f;
        return;
    }

    // 4. Out-of-bounds detection in OpenPlay
    if (state.play_state == PlayState::OpenPlay) {
        // Endline crossing (outside goal mouth or over crossbar) -> Corner or GoalKick
        if (std::abs(ball.position.x) > Pitch::HALF_LENGTH) {
            const uint8_t defending_team = ((ball.position.x > 0.0f) == (state.half == 1)) ? 1 : 0;
            state.ball.owner_slot = -1;
            state.shot_active = false;
            state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};

            if (ball.last_touch_team == static_cast<int8_t>(defending_team)) {
                // Corner Kick
                const uint8_t attacking_team = static_cast<uint8_t>(1 - defending_team);
                const float corner_x = (ball.position.x > 0.0f)
                    ? (Pitch::HALF_LENGTH - 0.3f)
                    : (-Pitch::HALF_LENGTH + 0.3f);
                const float corner_y = (ball.position.y >= 0.0f)
                    ? (Pitch::HALF_WIDTH - 0.3f)
                    : (-Pitch::HALF_WIDTH + 0.3f);

                state.play_state = PlayState::Corner;
                state.restart_team = static_cast<int8_t>(attacking_team);
                state.restart_spot = Vec2{corner_x, corner_y};
                state.ball.position = Vec3{corner_x, corner_y, 0.0f};
                state.restart_timer_ticks = 28;
                state.restart_taker_slot = select_nearest_outfield_taker(state, attacking_team, state.restart_spot);

                MatchEvent evt{};
                evt.tick = state.tick;
                evt.type = EventType::Corner;
                evt.primary_slot = state.restart_taker_slot;
                evt.location = state.restart_spot;
                events.push_back(evt);
            } else {
                // Goal Kick
                const float gk_x = (ball.position.x > 0.0f)
                    ? (Pitch::HALF_LENGTH - 5.5f)
                    : (-Pitch::HALF_LENGTH + 5.5f);
                const float gk_y = (ball.position.y >= 0.0f) ? 4.5f : -4.5f;

                state.play_state = PlayState::GoalKick;
                state.restart_team = static_cast<int8_t>(defending_team);
                state.restart_spot = Vec2{gk_x, gk_y};
                state.ball.position = Vec3{gk_x, gk_y, 0.0f};
                state.restart_timer_ticks = 28;
                state.restart_taker_slot = (defending_team == 0) ? 0 : 11;

                MatchEvent evt{};
                evt.tick = state.tick;
                evt.type = EventType::GoalKick;
                evt.primary_slot = state.restart_taker_slot;
                evt.location = state.restart_spot;
                events.push_back(evt);
            }
            return;
        }

        // Touchline crossing -> ThrowIn
        if (std::abs(ball.position.y) > Pitch::HALF_WIDTH) {
            const uint8_t restart_team = (ball.last_touch_team == 0) ? 1 : 0;
            const float throw_x = std::clamp(ball.position.x, -Pitch::HALF_LENGTH + 1.0f, Pitch::HALF_LENGTH - 1.0f);
            const float throw_y = (ball.position.y > 0.0f) ? Pitch::HALF_WIDTH : -Pitch::HALF_WIDTH;

            state.play_state = PlayState::ThrowIn;
            state.restart_team = static_cast<int8_t>(restart_team);
            state.restart_spot = Vec2{throw_x, throw_y};
            state.ball.owner_slot = -1;
            state.shot_active = false;
            state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
            state.ball.position = Vec3{
                throw_x,
                (throw_y > 0.0f) ? (Pitch::HALF_WIDTH - 0.20f) : (-Pitch::HALF_WIDTH + 0.20f),
                0.0f
            };
            state.restart_timer_ticks = 24;
            state.restart_taker_slot = select_nearest_outfield_taker(state, restart_team, state.restart_spot);

            MatchEvent evt{};
            evt.tick = state.tick;
            evt.type = EventType::ThrowIn;
            evt.primary_slot = state.restart_taker_slot;
            evt.location = state.restart_spot;
            events.push_back(evt);
            return;
        }
    }

    // 5. Set-piece countdown & restart execution (Kickoff, ThrowIn, GoalKick, Corner)
    if (state.play_state == PlayState::Kickoff ||
        state.play_state == PlayState::ThrowIn ||
        state.play_state == PlayState::GoalKick ||
        state.play_state == PlayState::Corner) {
        if (state.restart_timer_ticks > 0) {
            state.restart_timer_ticks--;
            // Pin ball cleanly to restart spot while players align
            state.ball.owner_slot = -1;
            state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
        }
        if (state.restart_timer_ticks == 0) {
            execute_restart(state, events);
        }
    }
}

} // namespace fm
