#pragma once
#include "fm/model/match_state.hpp"
#include "fm/model/match_event.hpp"
#include <vector>

namespace fm {

class MatchRules {
public:
    static void setup_kickoff(MatchState& state, uint8_t kicking_team);
    static void evaluate(MatchState& state, std::vector<MatchEvent>& events);
    static void execute_restart(MatchState& state, std::vector<MatchEvent>& events);
};

} // namespace fm
