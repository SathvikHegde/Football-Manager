#include "fm/recording/checkpoint_manager.hpp"

namespace fm {

void CheckpointManager::save_checkpoint(const MatchState& state) {
    checkpoints_.push_back(state);
}

const MatchState* CheckpointManager::get_nearest_checkpoint(uint32_t tick) const {
    const MatchState* best = nullptr;
    for (const auto& cp : checkpoints_) {
        if (cp.tick <= tick) {
            if (!best || cp.tick > best->tick) {
                best = &cp;
            }
        }
    }
    return best;
}

void CheckpointManager::truncate_after(uint32_t tick) {
    while (!checkpoints_.empty() && checkpoints_.back().tick > tick) {
        checkpoints_.pop_back();
    }
}

void CheckpointManager::clear() {
    checkpoints_.clear();
}

} // namespace fm

