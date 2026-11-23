#pragma once
#include <vector>
#include <cstdint>
#include "fm/model/match_state.hpp"

namespace fm {

class CheckpointManager {
public:
    static constexpr uint32_t CHECKPOINT_INTERVAL = 1200; // 1 match minute @ 20Hz

    CheckpointManager() = default;

    void save_checkpoint(const MatchState& state);
    [[nodiscard]] const MatchState* get_nearest_checkpoint(uint32_t tick) const;
    void truncate_after(uint32_t tick);
    void clear();
    [[nodiscard]] std::size_t count() const { return checkpoints_.size(); }

private:
    std::vector<MatchState> checkpoints_{};
};

} // namespace fm

