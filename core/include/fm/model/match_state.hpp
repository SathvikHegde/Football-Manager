#pragma once
#include <array>
#include <cstdint>
#include "fm/math/rng.hpp"
#include "fm/math/vec2.hpp"
#include "fm/math/vec3.hpp"
#include "fm/model/player.hpp"

namespace fm {

struct BallState {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    Vec3 spin_rad_s{0.0f, 0.0f, 0.0f};
    int8_t owner_slot{-1};           // -1 if loose in flight/ground
    int8_t last_touch_slot{-1};
    int8_t last_touch_team{-1};
    uint16_t claim_lock_ticks{0};    // Prevents instant re-claim by passer
};

enum class FormationId : uint8_t {
    F_442, F_433, F_4231, F_352, F_532
};

struct TacticalConfig {
    FormationId formation{FormationId::F_442};
    std::array<Vec2, 11> base_anchors_norm{}; // Normalized [0,1] pitch positions
    float defensive_line{0.5f};     // 0.0 (Deep Drop) to 1.0 (High Line)
    float pressing_intensity{0.5f}; // 0.0 (Stand Off) to 1.0 (Gegenpress)
    float passing_directness{0.5f}; // 0.0 (Short/Tiki-Taka) to 1.0 (Direct/Long Ball)
    float tempo{0.5f};              // 0.0 (Patient) to 1.0 (Fast Pace)
};

enum class PlayState : uint8_t {
    Kickoff, OpenPlay, ThrowIn, GoalKick, Corner, GoalScored, HalfTime, FullTime
};

struct MatchState {
    uint32_t tick{0};
    uint8_t half{1};
    uint8_t score_home{0};
    uint8_t score_away{0};
    PlayState play_state{PlayState::Kickoff};
    uint16_t restart_timer_ticks{0};
    int8_t restart_team{-1};
    Vec2 restart_spot{0.0f, 0.0f};
    BallState ball{};
    std::array<PlayerAgent, 22> players{};
    TacticalConfig home_tactics{};
    TacticalConfig away_tactics{};
    RngState rng{};

    // Stage 2: Match flow, transition tracking, and shot-flight state
    uint8_t kickoff_team{0};
    int8_t restart_taker_slot{-1};
    int8_t last_possessing_team{-1};
    uint32_t possession_change_tick{0};
    bool shot_active{false};
    int8_t shot_shooter_slot{-1};
    Vec2 shot_origin{0.0f, 0.0f};
    Vec3 shot_target_pos{0.0f, 0.0f, 0.0f};
    float shot_initial_speed{0.0f};
    uint16_t stuck_ball_ticks{0};
    Vec2 last_ball_check_pos{0.0f, 0.0f};
};

} // namespace fm
