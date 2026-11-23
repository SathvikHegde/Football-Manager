#pragma once
#include <cstdint>
#include "fm/math/vec2.hpp"
#include "fm/model/attributes.hpp"

namespace fm {

enum class ActionType : uint8_t {
    Idle, Move, Pass, Dribble, Shoot, Tackle, Intercept, Shield, Save
};

enum class RoleId : uint8_t {
    Goalkeeper, CenterBack, FullBack, Midfielder, Winger, Forward
};

struct PlayerAgent {
    uint8_t slot_index{0};           // 0..21 (0..10 Home, 11..21 Away; 0 and 11 are GKs)
    uint32_t player_id{0};
    uint8_t team_id{0};              // 0 = Home, 1 = Away
    RoleId role{RoleId::Midfielder};
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    float facing_rad{0.0f};
    float stamina_pct{1.0f};         // 1.0 (fully rested) -> 0.0 (exhausted)
    ActionType current_action{ActionType::Idle};
    uint16_t action_cooldown_ticks{0};
    Vec2 move_target{0.0f, 0.0f};
    PlayerAttributes attrs{};
};

} // namespace fm

