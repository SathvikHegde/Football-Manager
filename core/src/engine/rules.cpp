#include "fm/engine/rules.hpp"
#include "fm/engine/pitch.hpp"
#include <cmath>

namespace fm {

void MatchRules::evaluate(MatchState& state, std::vector<MatchEvent>& events) {
    const auto& ball = state.ball;

    // Half time and Full time state transitions
    if (state.tick == Pitch::TICKS_PER_HALF) {
        state.play_state = PlayState::HalfTime;
        MatchEvent evt{};
        evt.tick = state.tick;
        evt.type = EventType::HalfTime;
        events.push_back(evt);
        return;
    } else if (state.tick >= Pitch::TICKS_FULL_TIME) {
        state.play_state = PlayState::FullTime;
        MatchEvent evt{};
        evt.tick = state.tick;
        evt.type = EventType::FullTime;
        events.push_back(evt);
        return;
    }

    // Goal detection: |ball.pos.x| > 52.5 and |ball.pos.y| < 3.66 and ball.pos.z < 2.44
    if (std::abs(ball.position.x) > Pitch::HALF_LENGTH &&
        std::abs(ball.position.y) < Pitch::HALF_GOAL_WIDTH &&
        ball.position.z < Pitch::GOAL_HEIGHT) {

        MatchEvent goal_evt{};
        goal_evt.tick = state.tick;
        goal_evt.type = EventType::Goal;
        goal_evt.location = Vec2{ball.position.x, ball.position.y};

        if (ball.position.x > 0.0f) {
            state.score_home++;
            goal_evt.primary_slot = ball.last_touch_slot;
        } else {
            state.score_away++;
            goal_evt.primary_slot = ball.last_touch_slot;
        }

        events.push_back(goal_evt);
        state.play_state = PlayState::GoalScored;
        state.restart_timer_ticks = 60; // 3 seconds delay
        return;
    }

    // Out of bounds detection
    if (std::abs(ball.position.y) > Pitch::HALF_WIDTH) {
        if (state.play_state == PlayState::OpenPlay) {
            state.play_state = PlayState::ThrowIn;
            state.restart_team = (ball.last_touch_team == 0) ? 1 : 0;
            state.restart_spot = Vec2{ball.position.x, (ball.position.y > 0.0f ? Pitch::HALF_WIDTH : -Pitch::HALF_WIDTH)};
            
            MatchEvent evt{};
            evt.tick = state.tick;
            evt.type = EventType::ThrowIn;
            evt.location = state.restart_spot;
            events.push_back(evt);
        }
    }
}

} // namespace fm

