#include "fm/ai/utility_ai.hpp"
#include "fm/ai/tactical_anchors.hpp"
#include "fm/engine/pitch.hpp"
#include <algorithm>
#include <cmath>

namespace fm {
namespace AI {

namespace {

float point_to_segment_distance(const Vec2& p, const Vec2& a, const Vec2& b) {
    const Vec2 ab = b - a;
    const float len_sq = ab.length_sq();
    if (len_sq < 1e-6f) {
        return (p - a).length();
    }
    const float t = std::clamp((p - a).dot(ab) / len_sq, 0.0f, 1.0f);
    const Vec2 proj = a + ab * t;
    return (p - proj).length();
}

float nearest_opponent_distance(const MatchState& state, uint8_t team_id, const Vec2& pt, uint8_t* out_opp_slot = nullptr) {
    const int8_t opp_start = (team_id == 0) ? 11 : 0;
    float min_dist = 1e9f;
    uint8_t best_slot = static_cast<uint8_t>(opp_start);

    for (int8_t i = opp_start; i < opp_start + 11; ++i) {
        const float d = (state.players[static_cast<std::size_t>(i)].position - pt).length();
        if (d < min_dist) {
            min_dist = d;
            best_slot = static_cast<uint8_t>(i);
        }
    }
    if (out_opp_slot) {
        *out_opp_slot = best_slot;
    }
    return min_dist;
}

} // namespace

void evaluate_agent_utility(MatchState& state, uint8_t player_index) {
    if (player_index >= state.players.size()) return;
    auto& player = state.players[player_index];

    if (player.action_cooldown_ticks > 0) {
        player.current_action = ActionType::Idle;
        player.move_target = player.position;
        return;
    }

    const uint8_t team_id = player.team_id;
    const TacticalConfig& tactics = (team_id == 0) ? state.home_tactics : state.away_tactics;
    const float attack_dir = Pitch::attack_direction(team_id, state.half);
    const float sigma_decisions = 0.16f * (1.0f - player.attrs.norm(player.attrs.decisions));

    // =========================================================================
    // ON-BALL UTILITY EVALUATION
    // =========================================================================
    if (state.ball.owner_slot == static_cast<int8_t>(player_index)) {
        const bool is_gk = (player.role == RoleId::Goalkeeper);

        // 1. Shoot Utility
        float u_shoot = 0.0f;
        const Vec2 target_goal{Pitch::target_goal_x(team_id, state.half), 0.0f};
        const float dist_to_goal = (target_goal - player.position).length();
        const uint8_t opp_gk_slot = (team_id == 0) ? 11 : 0;
        const bool opp_gk_in_pos = (state.players[opp_gk_slot].position - target_goal).length() < 12.0f;
        const int8_t opp_start = (team_id == 0) ? 11 : 0;

        if (!is_gk && !(dist_to_goal > 30.0f && opp_gk_in_pos)) {
            const float cos_theta = std::max(0.0f, (target_goal.x - player.position.x) * attack_dir / std::max(dist_to_goal, 0.01f));
            const float raw_dist_term = std::max(0.0f, 1.0f - dist_to_goal / 30.0f);
            const float dist_term = 0.26f * raw_dist_term + 0.74f * raw_dist_term * raw_dist_term;
            const float skill_term = 0.54f + 0.46f * player.attrs.norm(player.attrs.shooting);

            float shot_lane_min = 1e9f;
            for (int8_t o = opp_start; o < opp_start + 11; ++o) {
                if (o == static_cast<int8_t>(opp_gk_slot)) continue;
                const auto& opp = state.players[static_cast<std::size_t>(o)];
                if ((opp.position - player.position).length_sq() > 100.0f) continue;
                const float d_seg = point_to_segment_distance(opp.position, player.position, target_goal);
                if (d_seg < shot_lane_min) shot_lane_min = d_seg;
            }
            const float lane_open = std::clamp((shot_lane_min - 0.5f) / 2.0f, 0.30f, 1.0f);
            const float angle_gate = (cos_theta < 0.52f && dist_to_goal > 11.0f) ? 0.32f : 1.0f;

            u_shoot = std::clamp(skill_term * cos_theta * dist_term * lane_open * angle_gate * 0.84f, 0.0f, 1.0f);
        }

        // 2. Pass Utility (evaluate all teammates)
        const float vision_max_dist = is_gk
            ? 50.0f
            : (16.0f + 30.0f * player.attrs.norm(player.attrs.vision));
        const float ideal_pass_dist = 8.0f + 15.0f * tactics.passing_directness;
        const int8_t mate_start = (team_id == 0) ? 0 : 11;

        int8_t best_pass_slot = -1;
        float best_pass_score = -1.0f;
        float best_pass_openness = 0.0f;

        uint8_t nearest_opp_slot = static_cast<uint8_t>(opp_start);
        const float nearest_opp_dist = nearest_opponent_distance(state, team_id, player.position, &nearest_opp_slot);

        for (int8_t j = mate_start; j < mate_start + 11; ++j) {
            if (j == static_cast<int8_t>(player_index)) continue;
            const auto& mate = state.players[static_cast<std::size_t>(j)];
            const float dist = (mate.position - player.position).length();
            if (dist < 3.5f || dist > vision_max_dist) continue;

            float min_lane_dist = 1e9f;
            const Vec2 pass_vec = mate.position - player.position;
            const float pass_len_sq = std::max(pass_vec.length_sq(), 1e-4f);
            for (int8_t o = opp_start; o < opp_start + 11; ++o) {
                const auto& opp = state.players[static_cast<std::size_t>(o)];
                const float along = (opp.position - player.position).dot(pass_vec) / pass_len_sq;
                if (along <= 0.04f || along >= 1.05f) continue;

                const float d_seg = point_to_segment_distance(opp.position, player.position, mate.position);
                const float effective_clearance = d_seg - std::clamp(dist * 0.03f, 0.0f, 0.9f);
                if (effective_clearance < min_lane_dist) min_lane_dist = effective_clearance;
            }

            const float recv_space_dist = nearest_opponent_distance(state, team_id, mate.position);
            const float needed_lane = 2.0f + dist * 0.060f;
            const float needed_recv = 2.2f + dist * 0.105f;
            const float lane_clear = std::clamp((min_lane_dist - needed_lane) / 4.0f, 0.0f, 1.0f);
            const float recv_space = std::clamp((recv_space_dist - needed_recv) / 4.8f, 0.0f, 1.0f);
            const float s_openness = 0.62f * lane_clear + 0.38f * recv_space;

            const float dx = (mate.position.x - player.position.x) * attack_dir;
            const float s_progression = std::clamp((dx + 10.0f) / 34.0f, 0.0f, 1.0f);
            const float s_distance = std::clamp(1.0f - std::abs(dist - ideal_pass_dist) / 20.0f, 0.0f, 1.0f);

            float pass_score = s_openness * 0.58f + s_progression * 0.22f + s_distance * 0.20f;
            if (min_lane_dist < needed_lane + 0.40f) {
                pass_score *= 0.12f; // Strongly reject risky/covered passing lanes
            }
            if (recv_space_dist < needed_recv + 0.30f) {
                pass_score *= 0.28f;
            }
            // Slight penalty for passing right next to touchlines/endlines
            if (std::abs(mate.position.y) > Pitch::HALF_WIDTH - 3.2f ||
                std::abs(mate.position.x) > Pitch::HALF_LENGTH - 3.8f) {
                pass_score *= 0.62f;
            }

            if (mate.role == RoleId::Goalkeeper && !(nearest_opp_dist < 3.5f && player.position.x * attack_dir < -14.0f)) {
                pass_score *= 0.45f;
            }
            if (j == state.ball.last_touch_slot && nearest_opp_dist > 4.0f) {
                pass_score *= 0.78f;
            }

            if (pass_score > best_pass_score) {
                best_pass_score = pass_score;
                best_pass_slot = j;
                best_pass_openness = s_openness;
            }
        }

        if (best_pass_slot < 0) {
            float fallback_best = 1e9f;
            for (int8_t j = mate_start; j < mate_start + 11; ++j) {
                if (j == static_cast<int8_t>(player_index)) continue;
                const float d = (state.players[static_cast<std::size_t>(j)].position - player.position).length();
                if (d < fallback_best) {
                    fallback_best = d;
                    best_pass_slot = j;
                }
            }
            best_pass_score = 0.25f;
            best_pass_openness = 0.20f;
        }

        // 3. Dribble Utility (8 radial direction rays)
        float best_dribble_score = 0.0f;
        Vec2 best_dribble_dir{attack_dir, 0.0f};
        constexpr float TWO_PI = 6.28318530718f;

        if (!is_gk) {
            const auto& nearest_def = state.players[nearest_opp_slot];
            const float skill_edge = std::clamp(
                0.50f + 0.30f * (player.attrs.norm(player.attrs.dribbling) + player.attrs.norm(player.attrs.pace))
                - 0.26f * nearest_def.attrs.norm(nearest_def.attrs.tackling),
                0.10f, 1.0f);

            for (int k = 0; k < 8; ++k) {
                const float angle = static_cast<float>(k) * (TWO_PI / 8.0f);
                const Vec2 dir{std::cos(angle), std::sin(angle)};
                const Vec2 ray_end = player.position + dir * 8.0f;

                float boundary_factor = 1.0f;
                if (std::abs(ray_end.x) > Pitch::HALF_LENGTH - 3.0f ||
                    std::abs(ray_end.y) > Pitch::HALF_WIDTH - 3.0f) {
                    boundary_factor = 0.15f;
                }

                float min_corridor = 1e9f;
                for (int8_t o = opp_start; o < opp_start + 11; ++o) {
                    const float d_seg = point_to_segment_distance(
                        state.players[static_cast<std::size_t>(o)].position,
                        player.position,
                        ray_end);
                    if (d_seg < min_corridor) min_corridor = d_seg;
                }

                const float open_space = std::clamp((min_corridor - 1.2f) / 5.5f, 0.0f, 1.0f);
                const float goal_align = 0.50f * (dir.x * attack_dir + 1.0f);
                const float ray_score = (0.48f * open_space + 0.32f * goal_align + 0.20f * skill_edge) * boundary_factor;

                if (ray_score > best_dribble_score) {
                    best_dribble_score = ray_score;
                    best_dribble_dir = dir;
                }
            }

            if (nearest_opp_dist > 3.2f && player.position.x * attack_dir < 29.0f) {
                best_dribble_score = std::min(0.88f, best_dribble_score + 0.18f);
            }
        } else {
            best_dribble_score = 0.05f;
        }

        // 4. Hold / Shield Utility
        float u_shield = is_gk ? 0.05f : 0.26f;
        if (!is_gk && best_pass_openness < 0.38f && nearest_opp_dist < 3.2f) {
            u_shield += 0.48f * (1.0f - best_pass_openness / 0.38f) * std::clamp(1.0f - nearest_opp_dist / 3.2f, 0.0f, 1.0f);
        }

        const float score_shoot   = (u_shoot > 0.0f) ? (u_shoot + state.rng.next_gaussian(0.0f, sigma_decisions)) : -10.0f;
        const float score_pass    = (best_pass_slot >= 0) ? (best_pass_score + state.rng.next_gaussian(0.0f, sigma_decisions)) : -10.0f;
        const float score_dribble = best_dribble_score + state.rng.next_gaussian(0.0f, sigma_decisions);
        const float score_shield  = u_shield + state.rng.next_gaussian(0.0f, sigma_decisions);

        if (score_shoot >= score_pass && score_shoot >= score_dribble && score_shoot >= score_shield) {
            player.current_action = ActionType::Shoot;
            player.action_target_slot = -1;
            player.action_target_pos = target_goal;
            player.move_target = player.position;
        } else if (score_pass >= score_dribble && score_pass >= score_shield) {
            player.current_action = ActionType::Pass;
            player.action_target_slot = best_pass_slot;
            player.action_target_pos = state.players[static_cast<std::size_t>(best_pass_slot)].position;
            player.move_target = player.position;
        } else if (score_dribble >= score_shield) {
            player.current_action = ActionType::Dribble;
            player.action_target_slot = -1;
            Vec2 dribble_target = player.position + best_dribble_dir * 6.0f;
            dribble_target.x = std::clamp(dribble_target.x, -Pitch::HALF_LENGTH + 2.5f, Pitch::HALF_LENGTH - 2.5f);
            dribble_target.y = std::clamp(dribble_target.y, -Pitch::HALF_WIDTH + 2.5f, Pitch::HALF_WIDTH - 2.5f);
            player.action_target_pos = dribble_target;
            player.move_target = dribble_target;
        } else {
            player.current_action = ActionType::Shield;
            player.action_target_slot = -1;
            player.move_target = player.position;
        }
        return;
    }

    // =========================================================================
    // OFF-BALL UTILITY EVALUATION (Outfield players; GK handled by GoalkeeperAI)
    // =========================================================================
    if (player.role == RoleId::Goalkeeper) {
        return;
    }

    const Vec2 ball_2d{state.ball.position.x, state.ball.position.y};
    const Vec2 ball_vel_2d{state.ball.velocity.x, state.ball.velocity.y};
    const bool team_in_possession =
        (state.ball.owner_slot >= 0 && ((state.ball.owner_slot < 11) ? 0 : 1) == team_id) ||
        (state.ball.owner_slot == -1 && state.ball.last_touch_team == static_cast<int8_t>(team_id));

    const Vec2 anchor = compute_dynamic_anchor(
        tactics, player.slot_index, ball_2d, team_in_possession, attack_dir, player.role);

    // Intended pass receiver tracks incoming pass cleanly toward ball trajectory
    if (state.ball.owner_slot == -1 && !state.shot_active) {
        if (team_in_possession && state.pass_target_slot == static_cast<int8_t>(player_index)) {
            Vec2 recv_pt = ball_2d + ball_vel_2d * 0.20f;
            if (ball_vel_2d.length_sq() > 1.0f) {
                const Vec2 b_dir = ball_vel_2d.normalized();
                const float proj = std::clamp((player.position - ball_2d).dot(b_dir), 0.0f, 25.0f);
                recv_pt = ball_2d + b_dir * proj;
            }
            recv_pt.x = std::clamp(recv_pt.x, -Pitch::HALF_LENGTH + 1.5f, Pitch::HALF_LENGTH - 1.5f);
            recv_pt.y = std::clamp(recv_pt.y, -Pitch::HALF_WIDTH + 1.5f, Pitch::HALF_WIDTH - 1.5f);
            player.current_action = ActionType::Intercept;
            player.move_target = recv_pt;
            return;
        }

        Vec2 intercept_pt = ball_2d + ball_vel_2d * (team_in_possession ? 0.20f : 0.35f);
        intercept_pt.x = std::clamp(intercept_pt.x, -Pitch::HALF_LENGTH + 1.0f, Pitch::HALF_LENGTH - 1.0f);
        intercept_pt.y = std::clamp(intercept_pt.y, -Pitch::HALF_WIDTH + 1.0f, Pitch::HALF_WIDTH - 1.0f);

        const float my_dist_sq = (player.position - intercept_pt).length_sq();
        int closer_mates = 0;
        const int8_t mate_start = (team_id == 0) ? 1 : 12;
        for (int8_t j = mate_start; j < mate_start + 10; ++j) {
            if (j == static_cast<int8_t>(player_index)) continue;
            const auto& mate = state.players[static_cast<std::size_t>(j)];
            if (mate.action_cooldown_ticks > 0) continue;
            const float mate_dist_sq = (mate.position - intercept_pt).length_sq();
            if (mate_dist_sq < my_dist_sq || (std::abs(mate_dist_sq - my_dist_sq) < 0.01f && j < static_cast<int8_t>(player_index))) {
                closer_mates++;
            }
        }

        if (closer_mates == 0 || (closer_mates == 1 && my_dist_sq < 100.0f)) {
            player.current_action = ActionType::Intercept;
            player.move_target = intercept_pt;
            return;
        }
    }

    // Cover Anchor utility
    Vec2 cover_target = anchor;
    if (!team_in_possession) {
        cover_target.x = std::clamp(anchor.x - attack_dir * 2.5f, -Pitch::HALF_LENGTH + 2.0f, Pitch::HALF_LENGTH - 2.0f);
    }
    const float dist_from_anchor = (player.position - cover_target).length();
    const float u_cover = 0.38f + 0.40f * std::clamp(dist_from_anchor / 12.0f, 0.0f, 1.0f) * (0.60f + 0.40f * player.attrs.norm(player.attrs.positioning));

    if (!team_in_possession && state.ball.owner_slot >= 0) {
        const auto& carrier = state.players[static_cast<std::size_t>(state.ball.owner_slot)];
        const float my_dist = (player.position - carrier.position).length();

        int closer_defenders = 0;
        const int8_t mate_start = (team_id == 0) ? 1 : 12;
        for (int8_t j = mate_start; j < mate_start + 10; ++j) {
            if (j == static_cast<int8_t>(player_index)) continue;
            const float d = (state.players[static_cast<std::size_t>(j)].position - carrier.position).length();
            if (d < my_dist || (std::abs(d - my_dist) < 0.01f && j < static_cast<int8_t>(player_index))) {
                closer_defenders++;
            }
        }

        float u_press = -10.0f;
        const int max_pressers = (tactics.pressing_intensity >= 0.35f) ? 2 : 1;
        if (closer_defenders < max_pressers) {
            const float dist_factor = std::clamp(1.0f - my_dist / 28.0f, 0.0f, 1.0f);
            const float rank_bonus = (closer_defenders == 0) ? 0.36f : 0.14f;
            u_press = 0.42f + rank_bonus + 0.35f * tactics.pressing_intensity * dist_factor;
        }

        const float score_cover = u_cover + state.rng.next_gaussian(0.0f, sigma_decisions);
        const float score_press = (u_press > 0.0f) ? (u_press + state.rng.next_gaussian(0.0f, sigma_decisions)) : -10.0f;

        if (score_press > score_cover) {
            Vec2 press_target = carrier.position + carrier.velocity * 0.20f;
            press_target.x = std::clamp(press_target.x, -Pitch::HALF_LENGTH + 1.0f, Pitch::HALF_LENGTH - 1.0f);
            press_target.y = std::clamp(press_target.y, -Pitch::HALF_WIDTH + 1.0f, Pitch::HALF_WIDTH - 1.0f);
            player.current_action = (my_dist < 2.2f) ? ActionType::Tackle : ActionType::Move;
            player.move_target = press_target;
        } else {
            player.current_action = ActionType::Move;
            player.move_target = cover_target;
        }
        return;
    }

    // In possession -> Support Carrier / Make Forward Run / Cover Anchor
    float u_support = -10.0f;
    Vec2 support_target = anchor;
    float u_forward_run = -10.0f;
    Vec2 run_target = anchor;

    if (state.ball.owner_slot >= 0) {
        const auto& carrier = state.players[static_cast<std::size_t>(state.ball.owner_slot)];
        const float dist_anchor_carrier = (anchor - carrier.position).length();
        const float dist_player_carrier = (player.position - carrier.position).length();

        if (dist_anchor_carrier < 34.0f) {
            Vec2 dir_to_anchor = (anchor - carrier.position).normalized();
            if (dir_to_anchor.length_sq() < 0.01f) {
                dir_to_anchor = Vec2{attack_dir, (player_index % 2 == 0) ? 0.7f : -0.7f}.normalized();
            }
            support_target = carrier.position + dir_to_anchor * 12.0f + Vec2{attack_dir * 2.5f, 0.0f};
            support_target.x = std::clamp(support_target.x, -Pitch::HALF_LENGTH + 3.0f, Pitch::HALF_LENGTH - 3.0f);
            support_target.y = std::clamp(support_target.y, -Pitch::HALF_WIDTH + 3.0f, Pitch::HALF_WIDTH - 3.0f);
            u_support = 0.45f + 0.30f * std::clamp(1.0f - std::abs(dist_player_carrier - 12.0f) / 14.0f, 0.0f, 1.0f);
        }

        const float carrier_forward = std::cos(carrier.facing_rad) * attack_dir;
        if (carrier_forward > 0.15f &&
            (player.role == RoleId::Forward || player.role == RoleId::Winger || player.role == RoleId::Midfielder) &&
            player.position.x * attack_dir < 40.0f) {
            run_target = Vec2{
                std::clamp(std::max(anchor.x * attack_dir, carrier.position.x * attack_dir + 11.0f) * attack_dir,
                           -Pitch::HALF_LENGTH + 3.5f, Pitch::HALF_LENGTH - 3.5f),
                std::clamp(anchor.y, -Pitch::HALF_WIDTH + 3.5f, Pitch::HALF_WIDTH - 3.5f)
            };
            const float role_affinity = (player.role == RoleId::Forward || player.role == RoleId::Winger) ? 0.34f : 0.16f;
            u_forward_run = 0.32f + role_affinity;
        }
    }

    const float score_cover   = u_cover + state.rng.next_gaussian(0.0f, sigma_decisions);
    const float score_support = (u_support > 0.0f) ? (u_support + state.rng.next_gaussian(0.0f, sigma_decisions)) : -10.0f;
    const float score_run     = (u_forward_run > 0.0f) ? (u_forward_run + state.rng.next_gaussian(0.0f, sigma_decisions)) : -10.0f;

    player.current_action = ActionType::Move;
    if (score_run >= score_support && score_run >= score_cover) {
        player.move_target = run_target;
    } else if (score_support >= score_cover) {
        player.move_target = support_target;
    } else {
        player.move_target = cover_target;
    }
}

} // namespace AI
} // namespace fm
