#pragma once
#include <cstdint>
#include "fm/math/vec2.hpp"

namespace fm {

enum class EventType : uint8_t {
    Kickoff, Pass, PassComplete, PassIntercepted, Dribble, TackleWon,
    Shot, ShotOnTarget, Save, Goal, OutTouchline, OutGoalLine,
    Corner, GoalKick, ThrowIn, HalfTime, FullTime, TacticalChange
};

struct MatchEvent {
    uint32_t tick{0};
    EventType type{EventType::Kickoff};
    int8_t primary_slot{-1};
    int8_t secondary_slot{-1};
    Vec2 location{0.0f, 0.0f};
    float xg_value{0.0f};

    [[nodiscard]] constexpr bool operator==(const MatchEvent& o) const = default;
};

} // namespace fm

