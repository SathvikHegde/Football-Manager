#pragma once
#include <cstdint>
#include <string_view>
#include "fm/model/match_state.hpp"

namespace fm {

TacticalConfig get_default_tactics(FormationId formation);
RoleId get_formation_role(FormationId formation, uint8_t local_slot);
[[nodiscard]] std::string_view formation_name(FormationId formation) noexcept;
[[nodiscard]] FormationId sanitize_formation(int formation_id) noexcept;

bool apply_tactical_change(MatchState& state,
                           uint8_t team_id,
                           FormationId formation,
                           float defensive_line,
                           float pressing_intensity);

bool apply_tactical_change(MatchState& state,
                           uint8_t team_id,
                           const TacticalConfig& new_tactics);

} // namespace fm
