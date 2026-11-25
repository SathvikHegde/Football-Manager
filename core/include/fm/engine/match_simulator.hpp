#pragma once
#include <vector>
#include <cstdint>
#include "fm/engine/pitch.hpp"
#include "fm/model/match_state.hpp"
#include "fm/model/match_event.hpp"
#include "fm/recording/checkpoint_manager.hpp"
#include "fm/recording/match_recording.hpp"

namespace fm {

class MatchSimulator {
public:
    MatchSimulator() = default;

    void reset(uint64_t seed = 12345);
    void initialize_match(MatchState& state,
                          uint64_t seed = 12345,
                          FormationId home_formation = FormationId::F_442,
                          FormationId away_formation = FormationId::F_442);
    void step_tick(MatchState& state);
    void simulate_match(MatchState& state, MatchRecording& recording, uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    void simulate_match(MatchState& state,
                        MatchRecording& recording,
                        CheckpointManager& checkpoints,
                        uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    void resume_simulation(MatchState& state,
                           MatchRecording& recording,
                           CheckpointManager& checkpoints,
                           uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    bool rollback_to_tick(MatchState& state,
                          MatchRecording& recording,
                          CheckpointManager& checkpoints,
                          uint32_t target_tick);
    bool rollback_and_resimulate(MatchState& state,
                                 MatchRecording& recording,
                                 CheckpointManager& checkpoints,
                                 uint32_t target_tick,
                                 uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    bool rollback_and_resimulate(MatchState& state,
                                 MatchRecording& recording,
                                 CheckpointManager& checkpoints,
                                 uint32_t target_tick,
                                 uint8_t team_id,
                                 FormationId formation,
                                 float defensive_line,
                                 float pressing_intensity,
                                 uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    bool rollback_and_resimulate(MatchState& state,
                                 MatchRecording& recording,
                                 CheckpointManager& checkpoints,
                                 uint32_t target_tick,
                                 const TacticalConfig* new_home_tactics,
                                 const TacticalConfig* new_away_tactics,
                                 uint32_t max_ticks = Pitch::TICKS_FULL_TIME);
    void run_sandbox_demo(MatchState& state, MatchRecording& recording, uint32_t ticks = 1000);

    [[nodiscard]] const std::vector<MatchEvent>& recent_events() const { return recent_events_; }
    void clear_events() { recent_events_.clear(); }

private:
    std::vector<MatchEvent> recent_events_{};
};

} // namespace fm
