#include "fm/engine/match_simulator.hpp"
#include "fm/engine/ball_physics.hpp"
#include "fm/engine/player_kinematics.hpp"
#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"
#include <cmath>
#include <array>
#include <algorithm>

namespace fm {

void MatchSimulator::reset(uint64_t /*seed*/) {
    recent_events_.clear();
    // Default reset for simulation state
}

void MatchSimulator::step_tick(MatchState& state) {
    recent_events_.clear();

    // Layer 1: Clock & Rules
    state.tick++;
    MatchRules::evaluate(state, recent_events_);

    // Layer 4: Ball Physics & Player Kinematics
    BallPhysics::step(state.ball, Pitch::DT);

    for (auto& player : state.players) {
        PlayerKinematics::step(player, player.move_target, Pitch::DT);
    }
}

void MatchSimulator::run_sandbox_demo(MatchState& state, MatchRecording& recording, uint32_t ticks) {
    recording.clear();
    recording.reserve(ticks);

    state = MatchState{};
    state.tick = 0;
    state.play_state = PlayState::OpenPlay;
    state.score_home = 0;
    state.score_away = 0;

    // Initial ball placement and trajectory (kept cleanly within pitch boundaries)
    state.ball.position = Vec3{0.0f, 0.0f, 0.0f};
    state.ball.velocity = Vec3{10.0f, 5.0f, 8.5f};
    state.ball.owner_slot = -1;
    state.ball.last_touch_slot = 9;
    state.ball.last_touch_team = 0;

    // Player orbital configuration (Center X, Center Y, Radius, Angular Speed, Initial Phase)
    struct OrbitConfig {
        Vec2 center;
        float radius;
        float speed;
        float phase;
        RoleId role;
    };

    constexpr float PI = 3.14159265358979323846f;
    std::array<OrbitConfig, 22> orbits = {{
        // Home Team (0..10)
        { {-48.0f,   0.0f},  3.0f, 0.40f, 0.0f * PI, RoleId::Goalkeeper }, // 0: GK
        { {-30.0f, -22.0f},  8.0f, 0.50f, 0.2f * PI, RoleId::FullBack },   // 1: LB
        { {-32.0f,  -8.0f},  7.0f, 0.60f, 0.4f * PI, RoleId::CenterBack }, // 2: CB
        { {-32.0f,   8.0f},  7.0f, 0.60f, 0.6f * PI, RoleId::CenterBack }, // 3: CB
        { {-30.0f,  22.0f},  8.0f, 0.50f, 0.8f * PI, RoleId::FullBack },   // 4: RB
        { {-12.0f, -20.0f},  9.0f, 0.70f, 1.0f * PI, RoleId::Winger },     // 5: LM
        { {-14.0f,  -6.0f}, 10.0f, 0.65f, 1.2f * PI, RoleId::Midfielder }, // 6: CM
        { {-14.0f,   6.0f}, 10.0f, 0.65f, 1.4f * PI, RoleId::Midfielder }, // 7: CM
        { {-12.0f,  20.0f},  9.0f, 0.70f, 1.6f * PI, RoleId::Winger },     // 8: RM
        { {  2.0f,  -8.0f}, 11.0f, 0.80f, 1.8f * PI, RoleId::Forward },    // 9: CF
        { {  2.0f,   8.0f}, 11.0f, 0.80f, 0.1f * PI, RoleId::Forward },    // 10: CF

        // Away Team (11..21)
        { { 48.0f,   0.0f},  3.0f, 0.40f, 1.0f * PI, RoleId::Goalkeeper }, // 11: GK
        { { 30.0f, -22.0f},  8.0f, 0.50f, 1.2f * PI, RoleId::FullBack },   // 12: RB
        { { 32.0f,  -8.0f},  7.0f, 0.60f, 1.4f * PI, RoleId::CenterBack }, // 13: CB
        { { 32.0f,   8.0f},  7.0f, 0.60f, 1.6f * PI, RoleId::CenterBack }, // 14: CB
        { { 30.0f,  22.0f},  8.0f, 0.50f, 1.8f * PI, RoleId::FullBack },   // 15: LB
        { { 12.0f, -20.0f},  9.0f, 0.70f, 0.0f * PI, RoleId::Winger },     // 16: RM
        { { 14.0f,  -6.0f}, 10.0f, 0.65f, 0.2f * PI, RoleId::Midfielder }, // 17: CM
        { { 14.0f,   6.0f}, 10.0f, 0.65f, 0.4f * PI, RoleId::Midfielder }, // 18: CM
        { { 12.0f,  20.0f},  9.0f, 0.70f, 0.6f * PI, RoleId::Winger },     // 19: LM
        { { -2.0f,  -8.0f}, 11.0f, 0.80f, 0.8f * PI, RoleId::Forward },    // 20: CF
        { { -2.0f,   8.0f}, 11.0f, 0.80f, 1.0f * PI, RoleId::Forward }     // 21: CF
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

    // Step simulation across ticks
    for (uint32_t t = 0; t < ticks; ++t) {
        float time_sec = static_cast<float>(t) * Pitch::DT;

        // Apply scripted kicks at specific intervals to exhibit multi-phase ball physics
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

        // Step 3D ball physics
        BallPhysics::step(state.ball, Pitch::DT);

        // Update 2D player kinematics along circular/orbital paths
        for (std::size_t i = 0; i < 22; ++i) {
            auto& p = state.players[i];
            const auto& orb = orbits[i];
            float angle = orb.phase + orb.speed * time_sec;
            p.move_target = orb.center + Vec2{orb.radius * std::cos(angle), orb.radius * std::sin(angle)};
            PlayerKinematics::step(p, p.move_target, Pitch::DT);
            p.stamina_pct = std::max(0.70f, 1.0f - (0.15f * (static_cast<float>(t) / static_cast<float>(ticks))));
        }

        // Capture frame snapshot into contiguous recording buffer
        recording.add_frame(FrameSnapshot::from_state(state));
        state.tick++;
    }
}

} // namespace fm

