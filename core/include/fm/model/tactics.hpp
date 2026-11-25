#pragma once
#include "fm/model/match_state.hpp"

namespace fm {

TacticalConfig get_default_tactics(FormationId formation);
RoleId get_formation_role(FormationId formation, uint8_t local_slot);

} // namespace fm
