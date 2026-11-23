#pragma once
#include <vector>
#include <cstdint>
#include "fm/model/match_state.hpp"
#include "fm/model/match_event.hpp"

namespace fm {

class MatchSimulator {
public:
    MatchSimulator() = default;

    void reset(uint64_t seed = 12345);
    void step_tick(MatchState& state);

    [[nodiscard]] const std::vector<MatchEvent>& recent_events() const { return recent_events_; }
    void clear_events() { recent_events_.clear(); }

private:
    std::vector<MatchEvent> recent_events_{};
};

} // namespace fm

