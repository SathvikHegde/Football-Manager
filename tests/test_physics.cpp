#include "test_framework.hpp"
#include "fm/math/vec2.hpp"
#include "fm/math/vec3.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/engine/ball_physics.hpp"

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
    // Horizontal velocity should dampen with turf friction 0.94
    REQUIRE(ball.velocity.x < 10.0f);
}

