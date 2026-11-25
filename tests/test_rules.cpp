#include "test_framework.hpp"
#include "fm/model/attributes.hpp"
#include "fm/model/tactics.hpp"
#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/ai/tactical_anchors.hpp"
#include "fm/ai/goalkeeper_ai.hpp"
#include <cmath>

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

TEST_CASE("Match Rules - Second Half Attack Polarity Goal Detection", "[rules]") {
    fm::MatchState state{};
    state.half = 2;
    state.tick = 60000;
    state.play_state = fm::PlayState::OpenPlay;
    // In half 2, Home attacks -X goal
    state.ball.position = fm::Vec3{-52.7f, 0.5f, 1.0f};
    state.ball.last_touch_slot = 9;

    std::vector<fm::MatchEvent> events;
    fm::MatchRules::evaluate(state, events);

    REQUIRE(state.score_home == 1);
    REQUIRE(state.score_away == 0);
    REQUIRE(state.play_state == fm::PlayState::GoalScored);
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

TEST_CASE("Match Rules - Endline Corner vs GoalKick", "[rules]") {
    // Ball crosses +52.5 outside goal mouth, last touched by defending Away (team 1) -> Corner for Home (team 0)
    {
        fm::MatchState state{};
        state.half = 1;
        state.tick = 300;
        state.play_state = fm::PlayState::OpenPlay;
        state.ball.position = fm::Vec3{52.8f, 10.0f, 0.2f};
        state.ball.last_touch_team = 1;

        std::vector<fm::MatchEvent> events;
        fm::MatchRules::evaluate(state, events);

        REQUIRE(state.play_state == fm::PlayState::Corner);
        REQUIRE(state.restart_team == 0);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == fm::EventType::Corner);
    }

    // Ball crosses +52.5 outside goal mouth, last touched by attacking Home (team 0) -> GoalKick for Away (team 1)
    {
        fm::MatchState state{};
        state.half = 1;
        state.tick = 350;
        state.play_state = fm::PlayState::OpenPlay;
        state.ball.position = fm::Vec3{52.8f, -12.0f, 0.5f};
        state.ball.last_touch_team = 0;

        std::vector<fm::MatchEvent> events;
        fm::MatchRules::evaluate(state, events);

        REQUIRE(state.play_state == fm::PlayState::GoalKick);
        REQUIRE(state.restart_team == 1);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == fm::EventType::GoalKick);
    }
}

TEST_CASE("Match Rules - HalfTime and FullTime Transitions", "[rules]") {
    fm::MatchState state{};
    state.half = 1;
    state.play_state = fm::PlayState::OpenPlay;
    state.tick = fm::Pitch::TICKS_PER_HALF;

    std::vector<fm::MatchEvent> events;
    fm::MatchRules::evaluate(state, events);
    REQUIRE(state.play_state == fm::PlayState::HalfTime);
    REQUIRE(state.half == 2);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == fm::EventType::HalfTime);

    events.clear();
    state.tick = fm::Pitch::TICKS_FULL_TIME;
    fm::MatchRules::evaluate(state, events);
    REQUIRE(state.play_state == fm::PlayState::FullTime);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].type == fm::EventType::FullTime);
}

TEST_CASE("Tactical Anchors - Width Compression and Wing Extension", "[rules]") {
    const auto tactics = fm::get_default_tactics(fm::FormationId::F_442);
    // Slot 5 is LM (norm.y = 0.15 -> -23.8m)
    const fm::Vec2 ball{0.0f, 0.0f};

    const fm::Vec2 in_pos = fm::AI::compute_dynamic_anchor(tactics, 5, ball, true, 1.0f, fm::RoleId::Winger);
    const fm::Vec2 out_pos = fm::AI::compute_dynamic_anchor(tactics, 5, ball, false, 1.0f, fm::RoleId::Winger);

    // In-possession winger extends near touchline (~28-30m), out-of-possession compresses 15% (~20m)
    REQUIRE(std::abs(in_pos.y) >= 27.5f);
    REQUIRE(std::abs(out_pos.y) < std::abs(in_pos.y));
    REQUIRE(std::abs(std::abs(out_pos.y) - (23.8f * 0.85f)) < 0.2f);
}

TEST_CASE("Goalkeeper AI - Angle Bisector & Shot Save Evaluation", "[rules]") {
    fm::MatchState state{};
    state.half = 1;
    state.players[0].slot_index = 0;
    state.players[0].team_id = 0;
    state.players[0].role = fm::RoleId::Goalkeeper;
    state.players[0].attrs.positioning = 15;
    state.players[0].attrs.acceleration = 15;

    // Ball on wing inside half -> GK shifts onto bisector toward ball side
    state.ball.position = fm::Vec3{-30.0f, 15.0f, 0.0f};
    fm::AI::update_goalkeeper_position(state, 0);
    REQUIRE(state.players[0].move_target.x > -52.5f);
    REQUIRE(state.players[0].move_target.x < -44.0f);
    REQUIRE(state.players[0].move_target.y > 0.0f);

    // Place GK near goal center; test saveable central shot vs unreachable top-corner rocket
    state.players[0].position = fm::Vec2{-51.0f, 0.0f};
    state.ball.position = fm::Vec3{-35.0f, 0.0f, 0.0f};
    REQUIRE(fm::AI::evaluate_shot_save(state, 0, fm::Vec3{-52.5f, 0.2f, 1.0f}, 22.0f));

    // Close-range rocket from 8m into extreme corner at 35 m/s should be unsaveable
    state.ball.position = fm::Vec3{-44.5f, 0.0f, 0.0f};
    REQUIRE_FALSE(fm::AI::evaluate_shot_save(state, 0, fm::Vec3{-52.5f, 3.60f, 2.30f}, 35.0f));
}
