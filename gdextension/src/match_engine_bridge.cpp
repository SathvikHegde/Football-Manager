#include "match_engine_bridge.hpp"
#include <godot_cpp/core/class_db.hpp>
#include "fm/engine/version.hpp"
#include "fm/engine/pitch.hpp"

namespace godot {

void MatchEngineBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("ping"), &MatchEngineBridge::ping);
    ClassDB::bind_method(D_METHOD("get_engine_version"), &MatchEngineBridge::get_engine_version);
    ClassDB::bind_method(D_METHOD("initialize_match", "seed"), &MatchEngineBridge::initialize_match);
    ClassDB::bind_method(D_METHOD("simulate_full_match"), &MatchEngineBridge::simulate_full_match);
    ClassDB::bind_method(D_METHOD("get_frame_data", "tick"), &MatchEngineBridge::get_frame_data);
    ClassDB::bind_method(D_METHOD("get_match_metadata"), &MatchEngineBridge::get_match_metadata);
    ClassDB::bind_method(D_METHOD("get_total_ticks"), &MatchEngineBridge::get_total_ticks);
    ClassDB::bind_method(D_METHOD("apply_tactical_change", "team_id", "formation_id", "defensive_line", "pressing"), &MatchEngineBridge::apply_tactical_change);
    ClassDB::bind_method(D_METHOD("rollback_and_resimulate", "target_tick"), &MatchEngineBridge::rollback_and_resimulate);
}

MatchEngineBridge::MatchEngineBridge() {
    initialize_match(initial_seed_);
}

MatchEngineBridge::~MatchEngineBridge() = default;

String MatchEngineBridge::ping() const {
    return String(fm::engine_status().data());
}

String MatchEngineBridge::get_engine_version() const {
    return String(fm::engine_version().data());
}

void MatchEngineBridge::initialize_match(uint64_t seed) {
    initial_seed_ = seed;
    simulator_.reset(seed);
    current_state_ = fm::MatchState{};
    current_state_.rng = fm::RngState(seed);
    recording_.clear();
    checkpoint_mgr_.clear();
}

void MatchEngineBridge::simulate_full_match() {
    recording_.reserve(fm::Pitch::TICKS_FULL_TIME);
    while (current_state_.tick < fm::Pitch::TICKS_FULL_TIME) {
        if (current_state_.tick % fm::CheckpointManager::CHECKPOINT_INTERVAL == 0) {
            checkpoint_mgr_.save_checkpoint(current_state_);
        }
        simulator_.step_tick(current_state_);
        recording_.add_frame(fm::FrameSnapshot::from_state(current_state_));
    }
}

PackedFloat32Array MatchEngineBridge::get_frame_data(int tick) const {
    PackedFloat32Array arr;
    arr.resize(static_cast<int64_t>(fm::FLOATS_PER_FRAME));
    if (tick >= 0 && static_cast<std::size_t>(tick) < recording_.frame_count()) {
        auto span = recording_.get_frame_span(static_cast<std::size_t>(tick));
        for (std::size_t i = 0; i < fm::FLOATS_PER_FRAME; ++i) {
            arr.set(static_cast<int64_t>(i), span[i]);
        }
    }
    return arr;
}

Dictionary MatchEngineBridge::get_match_metadata() const {
    Dictionary meta;
    meta["score_home"] = current_state_.score_home;
    meta["score_away"] = current_state_.score_away;
    meta["total_ticks"] = static_cast<int>(recording_.frame_count());
    meta["current_tick"] = static_cast<int>(current_state_.tick);
    meta["version"] = String(fm::engine_version().data());
    return meta;
}

int MatchEngineBridge::get_total_ticks() const {
    return static_cast<int>(recording_.frame_count());
}

void MatchEngineBridge::apply_tactical_change(int team_id, int formation_id, float defensive_line, float pressing) {
    auto& tactics = (team_id == 0) ? current_state_.home_tactics : current_state_.away_tactics;
    tactics.formation = static_cast<fm::FormationId>(formation_id);
    tactics.defensive_line = defensive_line;
    tactics.pressing_intensity = pressing;
}

void MatchEngineBridge::rollback_and_resimulate(int target_tick) {
    if (target_tick < 0) return;
    const auto* cp = checkpoint_mgr_.get_nearest_checkpoint(static_cast<uint32_t>(target_tick));
    if (cp) {
        current_state_ = *cp;
        while (current_state_.tick < static_cast<uint32_t>(target_tick)) {
            simulator_.step_tick(current_state_);
        }
        recording_.truncate(static_cast<std::size_t>(target_tick));
    }
}

} // namespace godot

