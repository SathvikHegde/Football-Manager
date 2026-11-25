#pragma once
#include <cstdint>
#include <string>
#include "fm/model/attributes.hpp"
#include "fm/model/match_state.hpp"
#include "fm/model/player.hpp"

namespace fm {

struct Team {
    uint8_t team_id{0};
    std::string name{"Default Team"};
    uint32_t primary_color{0xFF0000};
    uint32_t secondary_color{0xFFFFFF};
    uint8_t base_rating{12};
    FormationId default_formation{FormationId::F_442};
};

[[nodiscard]] PlayerAttributes make_role_attributes(RoleId role, uint8_t base_rating = 12) noexcept;
void apply_team_rating(MatchState& state, uint8_t team_id, uint8_t base_rating) noexcept;

} // namespace fm
