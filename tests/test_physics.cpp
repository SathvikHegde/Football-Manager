#include "test_framework.hpp"
#include "fm/math/vec2.hpp"
#include "fm/math/vec3.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/engine/ball_physics.hpp"
#include "fm/engine/player_kinematics.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/recording/match_recording.hpp"
#include <cmath>

TEST_CASE("Vec2 Math & Geometry", "[physics]") {
    fm::Vec2 a{3.0f, 4.0f};
    REQUIRE(a.length_sq() == 25.0f);
    REQUIRE(a.length() == 5.0f);

    fm::Vec2 n = a.normalized();
    REQUIRE(std::abs(n.x - 0.6f) < 0.0001f);
    REQUIRE(std::abs(n.y - 0.8f) < 0.0001f);
    REQUIRE(std::abs(n.length() - 1.0f) < 0.0001f);

    fm::Vec2 b{1.0f, 2.0f};
    fm::Vec2 c = a + b;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 6.0f);

    fm::Vec2 d = a - b;
    REQUIRE(d.x == 2.0f);
    REQUIRE(d.y == 2.0f);

    REQUIRE(a.dot(b) == 11.0f);
}

TEST_CASE("Vec3 Math & Geometry", "[physics]") {
    fm::Vec3 v{1.0f, 2.0f, 2.0f};
    REQUIRE(v.length_sq() == 9.0f);
    REQUIRE(v.length() == 3.0f);

    fm::Vec3 n = v.normalized();
    REQUIRE(std::abs(n.length() - 1.0f) < 0.0001f);
}

TEST_CASE("Pitch Dimensions & Boundaries", "[physics]") {
    REQUIRE(fm::Pitch::LENGTH == 105.0f);
    REQUIRE(fm::Pitch::WIDTH == 68.0f);
    REQUIRE(fm::Pitch::HALF_LENGTH == 52.5f);
    REQUIRE(fm::Pitch::HALF_WIDTH == 34.0f);
    REQUIRE(fm::Pitch::GOAL_WIDTH == 7.32f);
    REQUIRE(fm::Pitch::GOAL_HEIGHT == 2.44f);

    REQUIRE(fm::Pitch::is_in_bounds(fm::Vec2{0.0f, 0.0f}));
    REQUIRE(fm::Pitch::is_in_bounds(fm::Vec2{52.5f, 34.0f}));
    REQUIRE_FALSE(fm::Pitch::is_in_bounds(fm::Vec2{52.6f, 0.0f}));
    REQUIRE_FALSE(fm::Pitch::is_in_bounds(fm::Vec2{0.0f, 34.1f}));
}

TEST_CASE("Ball Physics - Ground Bounce & Turf Restitution", "[physics]") {
    fm::BallState ball{};
    ball.position = fm::Vec3{0.0f, 0.0f, 0.0f};
    ball.velocity = fm::Vec3{10.0f, 0.0f, -5.0f};

    fm::BallPhysics::step(ball, 0.05f);

    REQUIRE(ball.position.z >= 0.0f);
    // Vertical velocity should bounce upward with restitution 0.55
    REQUIRE(ball.velocity.z > 0.0f);
    REQUIRE(std::abs(ball.velocity.z - (5.0f * 0.55f)) < 0.35f);
    // Horizontal velocity should dampen with turf friction 0.94
    REQUIRE(ball.velocity.x < 10.0f);
}

TEST_CASE("Ball Physics - Parabolic Trajectory and Drag Decay", "[physics]") {
    fm::BallState ball{};
    ball.position = fm::Vec3{0.0f, 0.0f, 0.0f};
    ball.velocity = fm::Vec3{15.0f, 0.0f, 10.0f};

    float prev_x = ball.position.x;
    float max_z = 0.0f;
    for (int i = 0; i < 40; ++i) {
        fm::BallPhysics::step(ball, 0.05f);
        if (ball.position.z > max_z) max_z = ball.position.z;
        REQUIRE(ball.position.x > prev_x);
        prev_x = ball.position.x;
    }

    // Ball should reach an apex of around 4 to 5 meters
    REQUIRE(max_z > 3.5f);
    REQUIRE(max_z < 6.0f);
    // Drag should have reduced horizontal speed from initial 15 m/s
    REQUIRE(ball.velocity.x < 15.0f);
}

TEST_CASE("Ball Physics - Multi-Bounce Geometric Decay and Settlement", "[physics]") {
    fm::BallState ball{};
    ball.position = fm::Vec3{0.0f, 0.0f, 4.0f};
    ball.velocity = fm::Vec3{0.0f, 0.0f, 0.0f};

    int bounces = 0;
    float prev_vz = 0.0f;

    for (int i = 0; i < 150; ++i) {
        fm::BallPhysics::step(ball, 0.05f);
        if (prev_vz < 0.0f && ball.velocity.z > 0.0f) {
            bounces++;
        }
        prev_vz = ball.velocity.z;
    }

    // Multiple bounces before coming to rest
    REQUIRE(bounces >= 2);
    // Ball must settle cleanly on the ground
    REQUIRE(ball.position.z == 0.0f);
    REQUIRE(ball.velocity.z == 0.0f);

    // Further steps must keep ball stationary without micro-bounce jitter
    for (int i = 0; i < 20; ++i) {
        fm::BallPhysics::step(ball, 0.05f);
        REQUIRE(ball.position.z == 0.0f);
        REQUIRE(ball.velocity.z == 0.0f);
    }
}

TEST_CASE("Ball Physics - Turf Friction Rolling Decay", "[physics]") {
    fm::BallState ball{};
    ball.position = fm::Vec3{0.0f, 0.0f, 0.0f};
    ball.velocity = fm::Vec3{10.0f, 0.0f, 0.0f};

    for (int i = 0; i < 100; ++i) {
        fm::BallPhysics::step(ball, 0.05f);
        REQUIRE(ball.position.z == 0.0f);
        REQUIRE(ball.velocity.z == 0.0f);
    }

    // Rolling friction should bring ball to complete rest
    REQUIRE(ball.velocity.x == 0.0f);
    REQUIRE(ball.velocity.y == 0.0f);
}

TEST_CASE("Player Kinematics - Max Speed and Stamina Scaling", "[physics]") {
    fm::PlayerAttributes attrs_low{};
    attrs_low.pace = 1;
    // Pace 1 -> norm 0.0 -> (6.0 + 3.2 * 0) * (0.7 + 0.3 * 1.0) = 6.0 m/s
    REQUIRE(std::abs(fm::PlayerKinematics::calculate_max_speed(attrs_low, 1.0f) - 6.0f) < 0.01f);

    fm::PlayerAttributes attrs_high{};
    attrs_high.pace = 20;
    // Pace 20 -> norm 1.0 -> (6.0 + 3.2 * 1) * (0.7 + 0.3 * 1.0) = 9.2 m/s
    REQUIRE(std::abs(fm::PlayerKinematics::calculate_max_speed(attrs_high, 1.0f) - 9.2f) < 0.01f);

    // Exhausted player (stamina 0.0) -> 9.2 * 0.7 = 6.44 m/s
    REQUIRE(std::abs(fm::PlayerKinematics::calculate_max_speed(attrs_high, 0.0f) - 6.44f) < 0.01f);
}

TEST_CASE("Player Kinematics - Max Acceleration Clamping", "[physics]") {
    fm::PlayerAttributes attrs{};
    attrs.acceleration = 20; // norm 1.0 -> 2.5 + 2.5 * 1.0 = 5.0 m/s^2
    REQUIRE(std::abs(fm::PlayerKinematics::calculate_max_accel(attrs) - 5.0f) < 0.01f);

    fm::PlayerAgent player{};
    player.attrs = attrs;
    player.position = fm::Vec2{0.0f, 0.0f};
    player.velocity = fm::Vec2{0.0f, 0.0f};

    // Target far away (100m)
    fm::PlayerKinematics::step(player, fm::Vec2{100.0f, 0.0f}, 0.05f);

    // In 1 tick (0.05s) at max accel 5.0 m/s^2, velocity should be exactly 5.0 * 0.05 = 0.25 m/s
    REQUIRE(std::abs(player.velocity.x - 0.25f) < 0.001f);
    REQUIRE(player.velocity.y == 0.0f);
}

TEST_CASE("Player Kinematics - Angular Turn Rate Clamping", "[physics]") {
    fm::PlayerAgent player{};
    player.position = fm::Vec2{0.0f, 0.0f};
    player.velocity = fm::Vec2{5.0f, 0.0f};
    player.facing_rad = 0.0f; // Facing +X

    // Target directly backwards (-X)
    fm::PlayerKinematics::step(player, fm::Vec2{-50.0f, 0.0f}, 0.05f);

    // Max turn rate is 8.0 rad/s -> in 0.05s, turn is clamped to 0.40 rad
    REQUIRE(std::abs(std::abs(player.facing_rad) - 0.40f) < 0.02f);
}

TEST_CASE("Sandbox Demo Simulation - 1000 Ticks Complete Data Contract", "[physics]") {
    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;

    sim.run_sandbox_demo(state, recording, 1000);

    REQUIRE(recording.frame_count() == 1000);

    float max_ball_z = 0.0f;
    bool had_bounce_z0 = false;

    for (std::size_t t = 0; t < 1000; ++t) {
        auto span = recording.get_frame_span(t);
        REQUIRE(span.size() == fm::FLOATS_PER_FRAME);
        REQUIRE(span[0] == static_cast<float>(t));

        float bx = span[1];
        float by = span[2];
        float bz = span[3];

        if (bz > max_ball_z) max_ball_z = bz;
        if (bz == 0.0f) had_bounce_z0 = true;

        REQUIRE(bz >= 0.0f);
        REQUIRE(bx >= -55.0f);
        REQUIRE(bx <= 55.0f);
        REQUIRE(by >= -36.0f);
        REQUIRE(by <= 36.0f);

        // Verify 22 players
        for (std::size_t p = 0; p < 22; ++p) {
            std::size_t base = 5 + p * 4;
            float px = span[base + 0];
            float py = span[base + 1];
            float facing = span[base + 2];
            float stamina = span[base + 3];

            REQUIRE(px >= -55.0f);
            REQUIRE(px <= 55.0f);
            REQUIRE(py >= -36.0f);
            REQUIRE(py <= 36.0f);
            REQUIRE(facing >= -3.15f);
            REQUIRE(facing <= 3.15f);
            REQUIRE(stamina >= 0.69f);
            REQUIRE(stamina <= 1.0f);
        }
    }

    // Demonstrates elevated 3D ball motion and realistic ground touches
    REQUIRE(max_ball_z > 3.0f);
    REQUIRE(had_bounce_z0);
}

