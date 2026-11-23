#pragma once
#include <vector>
#include <cstdint>
#include "fm/model/match_state.hpp"
#include "fm/model/match_event.hpp"
#include "fm/recording/match_recording.hpp"

namespace fm {

class MatchSimulator {
public:
    MatchSimulator() = default;

    void reset(uint64_t seed = 12345);
    void step_tick(MatchState& state);
    void run_sandbox_demo(MatchState& state, MatchRecording& recording, uint32_t ticks = 1000);

    [[nodiscard]] const std::vector<MatchEvent>& recent_events() const { return recent_events_; }
    void clear_events() { recent_events_.clear(); }

private:
    std::vector<MatchEvent> recent_events_{};
};

} // namespace fm

