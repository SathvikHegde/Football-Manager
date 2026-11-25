#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
#include "fm/model/match_state.hpp"

namespace fm {

struct TacticalHistoryEntry {
    uint32_t tick{0};
    uint8_t team_id{0};
    TacticalConfig tactics{};
};

class CheckpointManager {
public:
    static constexpr uint32_t CHECKPOINT_INTERVAL = 1200; // 1 match minute @ 20Hz

    CheckpointManager() = default;

    void save_checkpoint(const MatchState& state);
    [[nodiscard]] const MatchState* get_nearest_checkpoint(uint32_t tick) const;
    [[nodiscard]] bool has_checkpoint_at(uint32_t tick) const;
    void truncate_after(uint32_t tick);
    void clear();
    [[nodiscard]] std::size_t count() const { return checkpoints_.size(); }
    [[nodiscard]] std::size_t memory_bytes() const;

    void record_tactical_change(uint32_t tick, uint8_t team_id, const TacticalConfig& tactics);
    void remove_tactical_change_at(uint32_t tick, uint8_t team_id);
    [[nodiscard]] const std::vector<TacticalHistoryEntry>& tactical_history() const { return tactical_history_; }

private:
    std::vector<MatchState> checkpoints_{};
    std::vector<TacticalHistoryEntry> tactical_history_{};
};

} // namespace fm

