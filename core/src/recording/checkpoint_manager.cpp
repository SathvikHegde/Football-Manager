#include "fm/recording/checkpoint_manager.hpp"
#include <algorithm>

namespace fm {

void CheckpointManager::save_checkpoint(const MatchState& state) {
    while (!checkpoints_.empty() && checkpoints_.back().tick >= state.tick) {
        checkpoints_.pop_back();
    }
    checkpoints_.push_back(state);
}

const MatchState* CheckpointManager::get_nearest_checkpoint(uint32_t tick) const {
    if (checkpoints_.empty()) {
        return nullptr;
    }

    auto it = std::upper_bound(
        checkpoints_.begin(),
        checkpoints_.end(),
        tick,
        [](uint32_t target, const MatchState& cp) {
            return target < cp.tick;
        });

    if (it == checkpoints_.begin()) {
        return nullptr;
    }
    --it;
    return &(*it);
}

bool CheckpointManager::has_checkpoint_at(uint32_t tick) const {
    auto it = std::lower_bound(
        checkpoints_.begin(),
        checkpoints_.end(),
        tick,
        [](const MatchState& cp, uint32_t target) {
            return cp.tick < target;
        });
    return it != checkpoints_.end() && it->tick == tick;
}

void CheckpointManager::truncate_after(uint32_t tick) {
    while (!checkpoints_.empty() && checkpoints_.back().tick > tick) {
        checkpoints_.pop_back();
    }
    std::erase_if(tactical_history_, [tick](const TacticalHistoryEntry& entry) {
        return entry.tick > tick;
    });
}

void CheckpointManager::clear() {
    checkpoints_.clear();
    tactical_history_.clear();
}

std::size_t CheckpointManager::memory_bytes() const {
    return checkpoints_.size() * sizeof(MatchState) +
           tactical_history_.size() * sizeof(TacticalHistoryEntry);
}

void CheckpointManager::record_tactical_change(uint32_t tick, uint8_t team_id, const TacticalConfig& tactics) {
    const uint8_t safe_team = (team_id == 0) ? 0 : 1;
    for (auto& entry : tactical_history_) {
        if (entry.tick == tick && entry.team_id == safe_team) {
            entry.tactics = tactics;
            return;
        }
    }
    TacticalHistoryEntry new_entry{tick, safe_team, tactics};
    auto pos = std::upper_bound(
        tactical_history_.begin(),
        tactical_history_.end(),
        tick,
        [](uint32_t target, const TacticalHistoryEntry& e) {
            return target < e.tick;
        });
    tactical_history_.insert(pos, new_entry);
}

void CheckpointManager::remove_tactical_change_at(uint32_t tick, uint8_t team_id) {
    const uint8_t safe_team = (team_id == 0) ? 0 : 1;
    std::erase_if(tactical_history_, [tick, safe_team](const TacticalHistoryEntry& entry) {
        return entry.tick == tick && entry.team_id == safe_team;
    });
}

} // namespace fm

