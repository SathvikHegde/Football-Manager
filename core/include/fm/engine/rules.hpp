#pragma once
#include "fm/model/match_state.hpp"
#include "fm/model/match_event.hpp"
#include <vector>

namespace fm {

class MatchRules {
public:
    static void evaluate(MatchState& state, std::vector<MatchEvent>& events);
};

} // namespace fm

