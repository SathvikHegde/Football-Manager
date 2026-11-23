#include "test_framework.hpp"
#include "fm/model/attributes.hpp"
#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"

TEST_CASE("Player Attributes Normalization Range", "[rules]") {
    fm::PlayerAttributes attrs{};

    // Attribute 1 maps to 0.0
    REQUIRE(attrs.norm(1) == 0.0f);

    // Attribute 20 maps to 1.0
    REQUIRE(attrs.norm(20) == 1.0f);

    // Attribute 10 maps to 9/19
    float norm10 = attrs.norm(10);
    REQUIRE(std::abs(norm10 - (9.0f / 19.0f)) < 0.0001f);

    // Out-of-bounds values are clamped
    REQUIRE(attrs.norm(0) == 0.0f);
    REQUIRE(attrs.norm(25) == 1.0f);
}

TEST_CASE("Match Rules - Goal Detection", "[rules]") {
    fm::MatchState state{};
    state.tick = 100;
    state.ball.position = fm::Vec3{52.6f, 1.0f, 1.5f}; // Inside away goal
    state.ball.last_touch_slot = 9; // Home striker

    std::vector<fm::MatchEvent> events;
    fm::MatchRules::evaluate(state, events);

    REQUIRE(state.score_home == 1);
    REQUIRE(state.score_away == 0);
    REQUIRE(state.play_state == fm::PlayState::GoalScored);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == fm::EventType::Goal);
}

TEST_CASE("Match Rules - Touchline Out of Bounds", "[rules]") {
    fm::MatchState state{};
    state.tick = 200;
    state.play_state = fm::PlayState::OpenPlay;
    state.ball.position = fm::Vec3{10.0f, 34.5f, 0.0f}; // Crosses touchline
    state.ball.last_touch_team = 0; // Touched by home team

    std::vector<fm::MatchEvent> events;
    fm::MatchRules::evaluate(state, events);

    REQUIRE(state.play_state == fm::PlayState::ThrowIn);
    REQUIRE(state.restart_team == 1); // Awarded to away team
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == fm::EventType::ThrowIn);
}

