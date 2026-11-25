#include "fm/engine/match_simulator.hpp"
#include "fm/engine/ball_physics.hpp"
#include "fm/engine/player_kinematics.hpp"
#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/model/tactics.hpp"
#include "fm/model/team.hpp"
#include "fm/ai/team_phase.hpp"
#include "fm/ai/tactical_anchors.hpp"
#include "fm/ai/utility_ai.hpp"
#include "fm/ai/goalkeeper_ai.hpp"
#include "fm/ai/execution.hpp"
#include <cmath>
#include <array>
#include <algorithm>

namespace fm {

namespace {

void ensure_players_initialized(MatchState& state) {
    // Check if MatchState was default-constructed without initialize_match()
    if (state.players[11].team_id == 1 && state.players[11].slot_index == 11) {
        return;
    }

    if (state.home_tactics.base_anchors_norm[0].x == 0.0f && state.home_tactics.base_anchors_norm[0].y == 0.0f) {
        state.home_tactics = get_default_tactics(state.home_tactics.formation);
    }
    if (state.away_tactics.base_anchors_norm[0].x == 0.0f && state.away_tactics.base_anchors_norm[0].y == 0.0f) {
        state.away_tactics = get_default_tactics(state.away_tactics.formation);
    }

    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state.players[i];
        p.slot_index = static_cast<uint8_t>(i);
        p.player_id = static_cast<uint32_t>(100 + i);
        p.team_id = (i < 11) ? 0 : 1;
        const auto& tactics = (p.team_id == 0) ? state.home_tactics : state.away_tactics;
        p.role = get_formation_role(tactics.formation, static_cast<uint8_t>(i % 11));
        p.stamina_pct = 1.0f;
    }
}

Vec2 enforce_exclusion_zone(const Vec2& target, const Vec2& spot, float min_radius) {
    Vec2 delta = target - spot;
    float d = delta.length();
    if (d < min_radius) {
        Vec2 push_dir = (d > 0.01f) ? delta.normalized() : Vec2{0.0f, 1.0f};
        if (spot.x > Pitch::HALF_LENGTH - 5.0f && push_dir.x > 0.0f) push_dir.x = -push_dir.x;
        if (spot.x < -Pitch::HALF_LENGTH + 5.0f && push_dir.x < 0.0f) push_dir.x = -push_dir.x;
        if (spot.y > Pitch::HALF_WIDTH - 5.0f && push_dir.y > 0.0f) push_dir.y = -push_dir.y;
        if (spot.y < -Pitch::HALF_WIDTH + 5.0f && push_dir.y < 0.0f) push_dir.y = -push_dir.y;
        push_dir = push_dir.normalized();
        Vec2 adjusted = spot + push_dir * min_radius;
        adjusted.x = std::clamp(adjusted.x, -Pitch::HALF_LENGTH + 1.5f, Pitch::HALF_LENGTH - 1.5f);
        adjusted.y = std::clamp(adjusted.y, -Pitch::HALF_WIDTH + 1.5f, Pitch::HALF_WIDTH - 1.5f);
        return adjusted;
    }
    return target;
}

void step_player_with_bounds(PlayerAgent& player, const Vec2& target_pos, float dt, bool is_ball_carrier) {
    PlayerKinematics::step(player, target_pos, dt);

    // Dribbling speed modulation for ball carrier
    if (is_ball_carrier) {
        const float carry_factor = 0.80f + 0.14f * player.attrs.norm(player.attrs.dribbling);
        const float max_carry_speed = PlayerKinematics::calculate_max_speed(player.attrs, player.stamina_pct) * carry_factor;
        const float speed = player.velocity.length();
        if (speed > max_carry_speed && speed > 0.01f) {
            player.velocity = player.velocity.normalized() * max_carry_speed;
        }
    }

    // Keep all agents safely within pitch apron bounds
    player.position.x = std::clamp(player.position.x, -Pitch::HALF_LENGTH - 1.5f, Pitch::HALF_LENGTH + 1.5f);
    player.position.y = std::clamp(player.position.y, -Pitch::HALF_WIDTH - 1.5f, Pitch::HALF_WIDTH + 1.5f);
}

void update_stamina(PlayerAgent& player, float dt) {
    const float speed_ratio = std::clamp(player.velocity.length() / 9.2f, 0.0f, 1.0f);
    const float stamina_resilience = 1.25f - 0.55f * player.attrs.norm(player.attrs.stamina);
    const float drain_rate = (0.000030f + 0.000060f * speed_ratio) * stamina_resilience;
    player.stamina_pct = std::clamp(player.stamina_pct - drain_rate * dt, 0.15f, 1.0f);
}

void sync_ball_to_carrier(MatchState& state) {
    if (state.ball.owner_slot < 0 || state.ball.owner_slot >= 22) return;
    const auto& carrier = state.players[static_cast<std::size_t>(state.ball.owner_slot)];
    const Vec2 dir{std::cos(carrier.facing_rad), std::sin(carrier.facing_rad)};
    state.ball.position = Vec3{carrier.position.x + dir.x * 0.55f, carrier.position.y + dir.y * 0.55f, 0.0f};
    state.ball.velocity = Vec3{carrier.velocity.x, carrier.velocity.y, 0.0f};
}

MatchEvent make_tactical_event(const MatchState& state, uint8_t team_id, const TacticalConfig& tactics) {
    MatchEvent evt{};
    evt.tick = state.tick;
    evt.type = EventType::TacticalChange;
    evt.primary_slot = (team_id == 0) ? 0 : 11;
    evt.secondary_slot = static_cast<int8_t>(tactics.formation);
    evt.location = Vec2{tactics.defensive_line, tactics.pressing_intensity};
    evt.xg_value = tactics.tempo;
    return evt;
}

void apply_recorded_tactics_at_tick(MatchState& state,
                                    const CheckpointManager& checkpoints,
                                    uint32_t tick,
                                    std::vector<MatchEvent>* out_events) {
    for (const auto& entry : checkpoints.tactical_history()) {
        if (entry.tick == tick) {
            if (apply_tactical_change(state, entry.team_id, entry.tactics) && out_events) {
                out_events->push_back(make_tactical_event(state, entry.team_id, entry.tactics));
            }
        }
    }
}

} // namespace

void MatchSimulator::reset(uint64_t /*seed*/) {
    recent_events_.clear();
}

void MatchSimulator::initialize_match(MatchState& state,
                                      uint64_t seed,
                                      FormationId home_formation,
                                      FormationId away_formation) {
    initialize_match(state, seed, home_formation, away_formation, 12, 12);
}

void MatchSimulator::initialize_match(MatchState& state,
                                      uint64_t seed,
                                      FormationId home_formation,
                                      FormationId away_formation,
                                      uint8_t home_rating,
                                      uint8_t away_rating) {
    recent_events_.clear();
    state = MatchState{};
    state.rng = RngState(seed);
    state.half = 1;
    state.tick = 0;
    state.home_tactics = get_default_tactics(home_formation);
    state.away_tactics = get_default_tactics(away_formation);

    const uint8_t clamped_home = std::clamp<uint8_t>(home_rating, 1, 20);
    const uint8_t clamped_away = std::clamp<uint8_t>(away_rating, 1, 20);

    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state.players[i];
        p.slot_index = static_cast<uint8_t>(i);
        p.player_id = static_cast<uint32_t>(100 + i);
        p.team_id = (i < 11) ? 0 : 1;
        const auto& tactics = (p.team_id == 0) ? state.home_tactics : state.away_tactics;
        const uint8_t team_rating = (p.team_id == 0) ? clamped_home : clamped_away;
        p.role = get_formation_role(tactics.formation, static_cast<uint8_t>(i % 11));
        p.stamina_pct = 1.0f;
        p.attrs = make_role_attributes(p.role, team_rating);
    }

    MatchRules::setup_kickoff(state, 0);
}

void MatchSimulator::step_tick(MatchState& state) {
    recent_events_.clear();

    if (state.play_state == PlayState::FullTime) {
        return;
    }

    ensure_players_initialized(state);

    // =========================================================================
    // LAYER 1: MATCH FLOW, CLOCK & RULES
    // =========================================================================
    state.tick++;
    MatchRules::evaluate(state, recent_events_);

    if (state.play_state == PlayState::FullTime) {
        return;
    }

    // Waiting states: GoalScored / HalfTime
    if (state.play_state == PlayState::GoalScored || state.play_state == PlayState::HalfTime) {
        BallPhysics::step(state.ball, Pitch::DT);
        if (state.play_state == PlayState::GoalScored) {
            // Goal net containment (net extends ~2.2m behind goal line)
            constexpr float NET_MAX_X = Pitch::HALF_LENGTH + 2.2f;
            constexpr float NET_MAX_Y = Pitch::HALF_GOAL_WIDTH - 0.15f;
            if (std::abs(state.ball.position.x) > NET_MAX_X) {
                state.ball.position.x = (state.ball.position.x > 0.0f) ? NET_MAX_X : -NET_MAX_X;
                state.ball.velocity.x *= -0.15f;
                state.ball.velocity.y *= 0.4f;
            }
            if (std::abs(state.ball.position.y) > NET_MAX_Y) {
                state.ball.position.y = (state.ball.position.y > 0.0f) ? NET_MAX_Y : -NET_MAX_Y;
                state.ball.velocity.y *= -0.15f;
            }
        }
        for (auto& p : state.players) {
            step_player_with_bounds(p, p.position, Pitch::DT, false);
        }
        return;
    }

    // Set-Piece waiting states: Kickoff, ThrowIn, GoalKick, Corner
    if (state.play_state == PlayState::Kickoff ||
        state.play_state == PlayState::ThrowIn ||
        state.play_state == PlayState::GoalKick ||
        state.play_state == PlayState::Corner) {

        const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};
        for (std::size_t i = 0; i < 22; ++i) {
            auto& p = state.players[i];
            if (static_cast<int8_t>(i) == state.restart_taker_slot) {
                step_player_with_bounds(p, state.restart_spot, Pitch::DT, false);
            } else {
                const auto& tactics = (p.team_id == 0) ? state.home_tactics : state.away_tactics;
                const float attack_dir = Pitch::attack_direction(p.team_id, state.half);
                Vec2 target = AI::compute_dynamic_anchor(
                    tactics, p.slot_index, ball_2d,
                    static_cast<int8_t>(p.team_id) == state.restart_team,
                    attack_dir, p.role);

                if (state.play_state == PlayState::Kickoff) {
                    if (target.x * attack_dir > -2.5f) {
                        target.x = -2.5f * attack_dir;
                    }
                }
                target = enforce_exclusion_zone(target, state.restart_spot, Pitch::CENTER_CIRCLE_RADIUS + 0.5f);
                step_player_with_bounds(p, target, Pitch::DT, false);
            }
        }
        return;
    }

    // =========================================================================
    // LAYER 2: TEAM PHASE CLASSIFICATION
    // =========================================================================
    (void)AI::classify_phase(state, 0);
    (void)AI::classify_phase(state, 1);

    // =========================================================================
    // LAYER 3: AGENT UTILITY AI & GOALKEEPER AI
    // =========================================================================
    for (std::size_t i = 0; i < 22; ++i) {
        if (state.players[i].action_cooldown_ticks > 0) {
            state.players[i].action_cooldown_ticks--;
        }
    }

    AI::update_goalkeeper_position(state, 0);
    AI::update_goalkeeper_position(state, 11);

    // GK evaluates distribution immediately when holding the ball
    if ((state.ball.owner_slot == 0 || state.ball.owner_slot == 11) &&
        state.players[static_cast<std::size_t>(state.ball.owner_slot)].action_cooldown_ticks == 0) {
        AI::evaluate_agent_utility(state, static_cast<uint8_t>(state.ball.owner_slot));
    }

    const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};
    for (uint8_t i = 0; i < 22; ++i) {
        if (i == 0 || i == 11) continue; // GKs already updated above
        auto& p = state.players[i];

        if (p.action_cooldown_ticks > 0) {
            p.current_action = ActionType::Idle;
            p.move_target = p.position;
            continue;
        }

        const bool is_owner = (state.ball.owner_slot == static_cast<int8_t>(i));
        const bool owner_needs_decision = is_owner &&
            (p.current_action != ActionType::Pass &&
             p.current_action != ActionType::Shoot &&
             p.current_action != ActionType::Dribble &&
             p.current_action != ActionType::Shield);
        const bool possession_interrupt = (state.possession_change_tick == state.tick);
        const bool loose_near_interrupt = (state.ball.owner_slot == -1) &&
            (((p.position - ball_2d).length_sq() < 100.0f && p.current_action != ActionType::Intercept) ||
             (state.pass_target_slot == static_cast<int8_t>(i) && p.current_action != ActionType::Intercept));
        const bool cadence_tick = ((state.tick + i) % 4 == 0);

        if (cadence_tick || owner_needs_decision || possession_interrupt || loose_near_interrupt) {
            AI::evaluate_agent_utility(state, i);
        }
    }

    // =========================================================================
    // LAYER 4: EXECUTION, BALL PHYSICS, KINEMATICS & CONTESTED POSSESSION
    // =========================================================================
    if (state.ball.owner_slot >= 0 && state.ball.owner_slot < 22) {
        const uint8_t owner = static_cast<uint8_t>(state.ball.owner_slot);
        if (state.players[owner].action_cooldown_ticks == 0) {
            AI::execute_action(state, owner, recent_events_);
        }
    }

    // Step 3D ball physics (only moves ball when owner_slot < 0)
    BallPhysics::step(state.ball, Pitch::DT);

    // Goalkeeper shot reaction & dive intercept (runs before next tick's goal-line evaluation)
    if (state.shot_active) {
        if (!AI::resolve_shot_intercept(state, 0, recent_events_)) {
            AI::resolve_shot_intercept(state, 11, recent_events_);
        }
        // If ball still crossed goal line and was not saved, clamp inside net wait? No, Layer 1 handles actual goals,
        // unless GK saved it above (which stops state.shot_active & redirects ball back into field).
        if (state.ball.velocity.length_sq() < 1.5f) {
            state.shot_active = false;
        }
    }

    // Outfield defender shot blocking (for shots in flight past initial kick)
    if (state.shot_active && state.ball.owner_slot == -1 && state.ball.position.z < 1.9f &&
        state.shot_shooter_slot >= 0 && state.shot_shooter_slot < 22) {
        const uint8_t shooter_team = state.players[static_cast<std::size_t>(state.shot_shooter_slot)].team_id;
        const Vec2 b2{state.ball.position.x, state.ball.position.y};
        const int8_t opp_start = (shooter_team == 0) ? 12 : 1; // Outfield defenders only
        for (int8_t d = opp_start; d < opp_start + 10; ++d) {
            auto& def = state.players[static_cast<std::size_t>(d)];
            if (def.action_cooldown_ticks > 0) continue;
            if ((def.position - b2).length_sq() < 0.72f) { // ~0.85m block radius
                const float block_dir_x = Pitch::attack_direction(def.team_id, state.half);
                const float deflect_y = (state.rng.next_float() - 0.5f) * 8.0f;
                state.ball.velocity = Vec3{block_dir_x * (4.0f + 3.0f * state.rng.next_float()), deflect_y, 1.4f};
                state.ball.last_touch_slot = d;
                state.ball.last_touch_team = static_cast<int8_t>(def.team_id);
                state.ball.claim_lock_ticks = 2;
                state.shot_active = false;
                state.pass_target_slot = -1;
                state.possession_change_tick = state.tick;
                def.action_cooldown_ticks = 4;

                MatchEvent block_evt{};
                block_evt.tick = state.tick;
                block_evt.type = EventType::TackleWon;
                block_evt.primary_slot = d;
                block_evt.secondary_slot = state.shot_shooter_slot;
                block_evt.location = def.position;
                recent_events_.push_back(block_evt);
                break;
            }
        }
    }

    // Step 2D kinematics and stamina for all 22 players
    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state.players[i];
        const bool is_carrier = (state.ball.owner_slot == static_cast<int8_t>(i));
        const ActionType prev_action = p.current_action;
        step_player_with_bounds(p, p.move_target, Pitch::DT, is_carrier);
        update_stamina(p, Pitch::DT);

        if (is_carrier && prev_action == ActionType::Dribble && (state.tick % 20 == 0)) {
            MatchEvent dribble_evt{};
            dribble_evt.tick = state.tick;
            dribble_evt.type = EventType::Dribble;
            dribble_evt.primary_slot = static_cast<int8_t>(i);
            dribble_evt.location = p.position;
            recent_events_.push_back(dribble_evt);
        }
    }

    // Contested Tackling (defender within 1.2m of ball carrier)
    if (state.ball.owner_slot >= 0 && state.ball.owner_slot < 22) {
        auto& carrier = state.players[static_cast<std::size_t>(state.ball.owner_slot)];
        const int8_t opp_start = (carrier.team_id == 0) ? 11 : 0;

        for (int8_t o = opp_start; o < opp_start + 11; ++o) {
            auto& def = state.players[static_cast<std::size_t>(o)];
            if (def.action_cooldown_ticks > 0) continue;
            if ((def.position - carrier.position).length() < 1.2f) {
                const float p_success = std::clamp(
                    0.50f + 0.92f * def.attrs.norm(def.attrs.tackling) - 0.86f * carrier.attrs.norm(carrier.attrs.dribbling),
                    0.06f, 0.94f);

                if (state.rng.next_float() < p_success) {
                    Vec2 deflect_dir = Vec2{std::cos(def.facing_rad), std::sin(def.facing_rad)};
                    if (deflect_dir.length_sq() < 0.01f) {
                        deflect_dir = Vec2{Pitch::attack_direction(def.team_id, state.half), 0.0f};
                    }
                    const float impulse = 4.0f + 3.0f * state.rng.next_float();
                    state.ball.owner_slot = -1;
                    state.ball.velocity = Vec3{deflect_dir.x * impulse, deflect_dir.y * impulse, 0.7f};
                    state.ball.claim_lock_ticks = 3;
                    state.ball.last_touch_slot = o;
                    state.ball.last_touch_team = static_cast<int8_t>(def.team_id);
                    state.shot_active = false;
                    state.possession_change_tick = state.tick;

                    carrier.action_cooldown_ticks = 10; // 0.5s stun
                    carrier.current_action = ActionType::Idle;

                    MatchEvent tackle_evt{};
                    tackle_evt.tick = state.tick;
                    tackle_evt.type = EventType::TackleWon;
                    tackle_evt.primary_slot = o;
                    tackle_evt.secondary_slot = static_cast<int8_t>(carrier.slot_index);
                    tackle_evt.location = carrier.position;
                    recent_events_.push_back(tackle_evt);
                } else {
                    def.action_cooldown_ticks = 14; // 0.7s recover
                    def.current_action = ActionType::Idle;
                }
                break; // One tackle contest resolved per tick
            }
        }
    }

    // Loose Ball Claim & Pass Completion / Interception
    if (state.ball.owner_slot == -1 && state.ball.claim_lock_ticks == 0 && state.ball.position.z < 1.8f) {
        const Vec2 b2{state.ball.position.x, state.ball.position.y};
        int8_t claimer = -1;
        float best_dist_sq = 1e9f;

        for (std::size_t i = 0; i < 22; ++i) {
            const auto& p = state.players[i];
            if (p.action_cooldown_ticks > 0) continue;

            const bool gk_in_box = (p.role == RoleId::Goalkeeper) && Pitch::is_in_own_penalty_area(b2, p.team_id, state.half);
            const bool is_intercept_attempt = (state.ball.last_touch_team >= 0 &&
                                               state.ball.last_touch_team != static_cast<int8_t>(p.team_id));
            const bool is_intended_receiver = (!state.shot_active &&
                                               state.pass_target_slot == static_cast<int8_t>(i) &&
                                               state.ball.last_touch_team == static_cast<int8_t>(p.team_id));
            float control_radius = is_intended_receiver
                ? (1.20f + 0.42f * p.attrs.norm(p.attrs.dribbling))
                : (0.98f + 0.34f * p.attrs.norm(p.attrs.dribbling));
            if (gk_in_box) {
                control_radius = 1.25f;
            } else if (is_intercept_attempt) {
                if (state.ball.position.z > 1.40f) continue;
                control_radius = 0.20f + 0.52f * p.attrs.norm(p.attrs.tackling);
            }

            const float d2 = (p.position - b2).length_sq();
            if (d2 < control_radius * control_radius && d2 < best_dist_sq) {
                best_dist_sq = d2;
                claimer = static_cast<int8_t>(i);
            }
        }

        if (claimer >= 0) {
            auto& p = state.players[static_cast<std::size_t>(claimer)];
            if (state.ball.last_touch_team == static_cast<int8_t>(p.team_id) &&
                state.ball.last_touch_slot >= 0 &&
                state.ball.last_touch_slot != claimer) {
                MatchEvent comp_evt{};
                comp_evt.tick = state.tick;
                comp_evt.type = EventType::PassComplete;
                comp_evt.primary_slot = state.ball.last_touch_slot;
                comp_evt.secondary_slot = claimer;
                comp_evt.location = p.position;
                recent_events_.push_back(comp_evt);
            } else if (state.ball.last_touch_team >= 0 &&
                       state.ball.last_touch_team != static_cast<int8_t>(p.team_id)) {
                MatchEvent int_evt{};
                int_evt.tick = state.tick;
                int_evt.type = EventType::PassIntercepted;
                int_evt.primary_slot = claimer;
                int_evt.secondary_slot = state.ball.last_touch_slot;
                int_evt.location = p.position;
                recent_events_.push_back(int_evt);
                state.possession_change_tick = state.tick;
            }

            state.ball.owner_slot = claimer;
            state.ball.last_touch_slot = claimer;
            state.ball.last_touch_team = static_cast<int8_t>(p.team_id);
            state.ball.velocity = Vec3{0.0f, 0.0f, 0.0f};
            state.ball.position.z = 0.0f;
            state.shot_active = false;
            state.pass_target_slot = -1;
            state.stuck_ball_ticks = 0;

            // Natural first touch: if not under immediate press, carry briefly before next decision
            if (p.role != RoleId::Goalkeeper) {
                const float adir = Pitch::attack_direction(p.team_id, state.half);
                const int8_t opp_start = (p.team_id == 0) ? 11 : 0;
                float min_opp_sq = 1e9f;
                for (int8_t o = opp_start; o < opp_start + 11; ++o) {
                    const float od2 = (state.players[static_cast<std::size_t>(o)].position - p.position).length_sq();
                    if (od2 < min_opp_sq) min_opp_sq = od2;
                }
                if (min_opp_sq > 6.25f) {
                    p.current_action = ActionType::Dribble;
                    p.move_target = Vec2{
                        std::clamp(p.position.x + adir * 4.0f, -Pitch::HALF_LENGTH + 2.5f, Pitch::HALF_LENGTH - 2.5f),
                        p.position.y
                    };
                }
            }
            if (state.last_possessing_team != static_cast<int8_t>(p.team_id)) {
                state.last_possessing_team = static_cast<int8_t>(p.team_id);
                state.possession_change_tick = state.tick;
            }
        }
    }

    // Deadlock guard: prevent a stationary loose ball from ever stalling play indefinitely
    if (state.ball.owner_slot == -1) {
        const Vec2 b2{state.ball.position.x, state.ball.position.y};
        if ((b2 - state.last_ball_check_pos).length_sq() < 0.01f && state.ball.velocity.length_sq() < 0.1f) {
            state.stuck_ball_ticks++;
            if (state.stuck_ball_ticks > 120) {
                // Assign closest non-stunned player to kick/move ball forward
                int8_t nearest = 0;
                float best_d2 = 1e9f;
                for (int8_t i = 0; i < 22; ++i) {
                    const float d2 = (state.players[static_cast<std::size_t>(i)].position - b2).length_sq();
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        nearest = i;
                    }
                }
                state.players[static_cast<std::size_t>(nearest)].move_target = b2;
                state.players[static_cast<std::size_t>(nearest)].action_cooldown_ticks = 0;
                if (best_d2 < 2.25f) {
                    state.ball.owner_slot = nearest;
                    state.ball.last_touch_slot = nearest;
                    state.ball.last_touch_team = static_cast<int8_t>(state.players[static_cast<std::size_t>(nearest)].team_id);
                    state.stuck_ball_ticks = 0;
                }
            }
        } else {
            state.stuck_ball_ticks = 0;
            state.last_ball_check_pos = b2;
        }
    } else {
        state.stuck_ball_ticks = 0;
    }

    sync_ball_to_carrier(state);
}

void MatchSimulator::simulate_match(MatchState& state, MatchRecording& recording, uint32_t max_ticks) {
    recording.clear();
    recording.reserve(max_ticks);

    while (state.tick < max_ticks && state.play_state != PlayState::FullTime) {
        step_tick(state);
        recording.add_frame(FrameSnapshot::from_state(state), state);
        if (!recent_events_.empty()) {
            recording.add_events(recent_events_);
        }
    }
}

void MatchSimulator::simulate_match(MatchState& state,
                                    MatchRecording& recording,
                                    CheckpointManager& checkpoints,
                                    uint32_t max_ticks) {
    recording.clear();
    checkpoints.clear();
    resume_simulation(state, recording, checkpoints, max_ticks);
}

void MatchSimulator::simulate_headless(MatchState& state,
                                       MatchStats& stats,
                                       uint32_t max_ticks) {
    stats.home_formation = state.home_tactics.formation;
    stats.away_formation = state.away_tactics.formation;
    stats.total_ticks = 0;
    stats.open_play_ticks = 0;
    stats.home = TeamMatchStats{};
    stats.away = TeamMatchStats{};

    while (state.tick < max_ticks && state.play_state != PlayState::FullTime) {
        step_tick(state);
        stats.accumulate_tick(state);
        if (!recent_events_.empty()) {
            stats.accumulate_events(recent_events_);
        }
    }
    stats.finalize(state);
}

void MatchSimulator::resume_simulation(MatchState& state,
                                       MatchRecording& recording,
                                       CheckpointManager& checkpoints,
                                       uint32_t max_ticks) {
    recording.reserve(max_ticks);

    while (state.tick < max_ticks && state.play_state != PlayState::FullTime) {
        if ((state.tick % CheckpointManager::CHECKPOINT_INTERVAL == 0) &&
            !checkpoints.has_checkpoint_at(state.tick)) {
            checkpoints.save_checkpoint(state);
        }
        step_tick(state);
        recording.add_frame(FrameSnapshot::from_state(state), state);
        if (!recent_events_.empty()) {
            recording.add_events(recent_events_);
        }
    }
}

bool MatchSimulator::rollback_to_tick(MatchState& state,
                                      MatchRecording& recording,
                                      CheckpointManager& checkpoints,
                                      uint32_t target_tick) {
    const uint32_t clamped_tick = std::min(target_tick, Pitch::TICKS_FULL_TIME);
    const MatchState* cp = checkpoints.get_nearest_checkpoint(clamped_tick);
    if (!cp) {
        return false;
    }

    const uint32_t cp_tick = cp->tick;
    state = *cp;
    checkpoints.truncate_after(clamped_tick);

    while (state.tick < clamped_tick && state.play_state != PlayState::FullTime) {
        apply_recorded_tactics_at_tick(state, checkpoints, state.tick, nullptr);
        if (state.tick > cp_tick &&
            (state.tick % CheckpointManager::CHECKPOINT_INTERVAL == 0) &&
            !checkpoints.has_checkpoint_at(state.tick)) {
            checkpoints.save_checkpoint(state);
        }
        step_tick(state);
    }

    recent_events_.clear();
    recording.truncate(static_cast<std::size_t>(clamped_tick));
    return true;
}

bool MatchSimulator::rollback_and_resimulate(MatchState& state,
                                             MatchRecording& recording,
                                             CheckpointManager& checkpoints,
                                             uint32_t target_tick,
                                             uint32_t max_ticks) {
    return rollback_and_resimulate(state, recording, checkpoints, target_tick, nullptr, nullptr, max_ticks);
}

bool MatchSimulator::rollback_and_resimulate(MatchState& state,
                                             MatchRecording& recording,
                                             CheckpointManager& checkpoints,
                                             uint32_t target_tick,
                                             uint8_t team_id,
                                             FormationId formation,
                                             float defensive_line,
                                             float pressing_intensity,
                                             uint32_t max_ticks) {
    const uint32_t clamped_tick = std::min(target_tick, max_ticks);
    const uint8_t safe_team = (team_id == 0) ? 0 : 1;

    // Remove any previous tactical override on this team at clamped_tick before rewinding
    // so we compare against the baseline tactics active before clamped_tick.
    checkpoints.remove_tactical_change_at(clamped_tick, safe_team);

    if (!rollback_to_tick(state, recording, checkpoints, clamped_tick)) {
        return false;
    }

    std::vector<MatchEvent> tactical_events;
    apply_recorded_tactics_at_tick(state, checkpoints, state.tick, &tactical_events);

    if (apply_tactical_change(state, safe_team, formation, defensive_line, pressing_intensity)) {
        const TacticalConfig& applied = (safe_team == 0) ? state.home_tactics : state.away_tactics;
        checkpoints.record_tactical_change(state.tick, safe_team, applied);
        tactical_events.push_back(make_tactical_event(state, safe_team, applied));
    }

    if (!tactical_events.empty()) {
        recording.add_events(tactical_events);
    }

    resume_simulation(state, recording, checkpoints, max_ticks);
    return true;
}

bool MatchSimulator::rollback_and_resimulate(MatchState& state,
                                             MatchRecording& recording,
                                             CheckpointManager& checkpoints,
                                             uint32_t target_tick,
                                             const TacticalConfig* new_home_tactics,
                                             const TacticalConfig* new_away_tactics,
                                             uint32_t max_ticks) {
    const uint32_t clamped_tick = std::min(target_tick, max_ticks);

    if (new_home_tactics) {
        checkpoints.remove_tactical_change_at(clamped_tick, 0);
    }
    if (new_away_tactics) {
        checkpoints.remove_tactical_change_at(clamped_tick, 1);
    }

    if (!rollback_to_tick(state, recording, checkpoints, clamped_tick)) {
        return false;
    }

    std::vector<MatchEvent> tactical_events;
    apply_recorded_tactics_at_tick(state, checkpoints, state.tick, &tactical_events);

    if (new_home_tactics && apply_tactical_change(state, 0, *new_home_tactics)) {
        checkpoints.record_tactical_change(state.tick, 0, state.home_tactics);
        tactical_events.push_back(make_tactical_event(state, 0, state.home_tactics));
    }
    if (new_away_tactics && apply_tactical_change(state, 1, *new_away_tactics)) {
        checkpoints.record_tactical_change(state.tick, 1, state.away_tactics);
        tactical_events.push_back(make_tactical_event(state, 1, state.away_tactics));
    }

    if (!tactical_events.empty()) {
        recording.add_events(tactical_events);
    }

    resume_simulation(state, recording, checkpoints, max_ticks);
    return true;
}

void MatchSimulator::run_sandbox_demo(MatchState& state, MatchRecording& recording, uint32_t ticks) {
    recording.clear();
    recording.reserve(ticks);

    state = MatchState{};
    state.tick = 0;
    state.play_state = PlayState::OpenPlay;
    state.score_home = 0;
    state.score_away = 0;

    state.ball.position = Vec3{0.0f, 0.0f, 0.0f};
    state.ball.velocity = Vec3{10.0f, 5.0f, 8.5f};
    state.ball.owner_slot = -1;
    state.ball.last_touch_slot = 9;
    state.ball.last_touch_team = 0;

    struct OrbitConfig {
        Vec2 center;
        float radius;
        float speed;
        float phase;
        RoleId role;
    };

    constexpr float PI = 3.14159265358979323846f;
    std::array<OrbitConfig, 22> orbits = {{
        { {-48.0f,   0.0f},  3.0f, 0.40f, 0.0f * PI, RoleId::Goalkeeper },
        { {-30.0f, -22.0f},  8.0f, 0.50f, 0.2f * PI, RoleId::FullBack },
        { {-32.0f,  -8.0f},  7.0f, 0.60f, 0.4f * PI, RoleId::CenterBack },
        { {-32.0f,   8.0f},  7.0f, 0.60f, 0.6f * PI, RoleId::CenterBack },
        { {-30.0f,  22.0f},  8.0f, 0.50f, 0.8f * PI, RoleId::FullBack },
        { {-12.0f, -20.0f},  9.0f, 0.70f, 1.0f * PI, RoleId::Winger },
        { {-14.0f,  -6.0f}, 10.0f, 0.65f, 1.2f * PI, RoleId::Midfielder },
        { {-14.0f,   6.0f}, 10.0f, 0.65f, 1.4f * PI, RoleId::Midfielder },
        { {-12.0f,  20.0f},  9.0f, 0.70f, 1.6f * PI, RoleId::Winger },
        { {  2.0f,  -8.0f}, 11.0f, 0.80f, 1.8f * PI, RoleId::Forward },
        { {  2.0f,   8.0f}, 11.0f, 0.80f, 0.1f * PI, RoleId::Forward },

        { { 48.0f,   0.0f},  3.0f, 0.40f, 1.0f * PI, RoleId::Goalkeeper },
        { { 30.0f, -22.0f},  8.0f, 0.50f, 1.2f * PI, RoleId::FullBack },
        { { 32.0f,  -8.0f},  7.0f, 0.60f, 1.4f * PI, RoleId::CenterBack },
        { { 32.0f,   8.0f},  7.0f, 0.60f, 1.6f * PI, RoleId::CenterBack },
        { { 30.0f,  22.0f},  8.0f, 0.50f, 1.8f * PI, RoleId::FullBack },
        { { 12.0f, -20.0f},  9.0f, 0.70f, 0.0f * PI, RoleId::Winger },
        { { 14.0f,  -6.0f}, 10.0f, 0.65f, 0.2f * PI, RoleId::Midfielder },
        { { 14.0f,   6.0f}, 10.0f, 0.65f, 0.4f * PI, RoleId::Midfielder },
        { { 12.0f,  20.0f},  9.0f, 0.70f, 0.6f * PI, RoleId::Winger },
        { { -2.0f,  -8.0f}, 11.0f, 0.80f, 0.8f * PI, RoleId::Forward },
        { { -2.0f,   8.0f}, 11.0f, 0.80f, 1.0f * PI, RoleId::Forward }
    }};

    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state.players[i];
        p.slot_index = static_cast<uint8_t>(i);
        p.player_id = static_cast<uint32_t>(100 + i);
        p.team_id = (i < 11) ? 0 : 1;
        p.role = orbits[i].role;
        p.stamina_pct = 1.0f;
        p.attrs.pace = static_cast<uint8_t>(12 + (i % 6));
        p.attrs.acceleration = static_cast<uint8_t>(12 + ((i + 2) % 6));
        p.attrs.stamina = 15;
        p.attrs.decisions = 12;

        const auto& orb = orbits[i];
        p.position = orb.center + Vec2{orb.radius * std::cos(orb.phase), orb.radius * std::sin(orb.phase)};
        p.velocity = Vec2{0.0f, 0.0f};
        p.facing_rad = orb.phase + 0.5f * PI;
        while (p.facing_rad > PI) p.facing_rad -= 2.0f * PI;
        while (p.facing_rad < -PI) p.facing_rad += 2.0f * PI;
        p.move_target = p.position;
    }

    for (uint32_t t = 0; t < ticks; ++t) {
        float time_sec = static_cast<float>(t) * Pitch::DT;

        if (t == 180) {
            state.ball.position.z = 0.0f;
            state.ball.velocity = Vec3{-15.0f, -6.0f, 6.0f};
        } else if (t == 380) {
            state.ball.position.z = 0.0f;
            state.ball.velocity = Vec3{12.0f, -12.0f, 7.0f};
        } else if (t == 580) {
            state.ball.position.z = 0.0f;
            state.ball.velocity = Vec3{-14.0f, 12.0f, 6.5f};
        } else if (t == 780) {
            state.ball.position.z = 0.0f;
            state.ball.velocity = Vec3{12.0f, -6.0f, 6.5f};
        }

        BallPhysics::step(state.ball, Pitch::DT);

        for (std::size_t i = 0; i < 22; ++i) {
            auto& p = state.players[i];
            const auto& orb = orbits[i];
            float angle = orb.phase + orb.speed * time_sec;
            p.move_target = orb.center + Vec2{orb.radius * std::cos(angle), orb.radius * std::sin(angle)};
            PlayerKinematics::step(p, p.move_target, Pitch::DT);
            p.stamina_pct = std::max(0.70f, 1.0f - (0.15f * (static_cast<float>(t) / static_cast<float>(ticks))));
        }

        recording.add_frame(FrameSnapshot::from_state(state), state);
        state.tick++;
    }
}

} // namespace fm
