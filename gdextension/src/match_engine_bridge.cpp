#include "match_engine_bridge.hpp"
#include <godot_cpp/core/class_db.hpp>
#include "fm/engine/version.hpp"
#include "fm/engine/pitch.hpp"
#include <cstdio>
#include <string>

namespace godot {

namespace {

const char* play_state_to_str(fm::PlayState ps) {
    switch (ps) {
        case fm::PlayState::Kickoff:    return "Kickoff";
        case fm::PlayState::OpenPlay:   return "Open Play";
        case fm::PlayState::ThrowIn:    return "Throw-In";
        case fm::PlayState::GoalKick:   return "Goal Kick";
        case fm::PlayState::Corner:     return "Corner";
        case fm::PlayState::GoalScored: return "GOAL!";
        case fm::PlayState::HalfTime:   return "Half-Time";
        case fm::PlayState::FullTime:   return "Full-Time";
        default:                        return "Unknown";
    }
}

const char* event_type_to_str(fm::EventType et) {
    switch (et) {
        case fm::EventType::Kickoff:         return "Kickoff";
        case fm::EventType::Pass:            return "Pass";
        case fm::EventType::PassComplete:    return "PassComplete";
        case fm::EventType::PassIntercepted: return "PassIntercepted";
        case fm::EventType::Dribble:         return "Dribble";
        case fm::EventType::TackleWon:       return "TackleWon";
        case fm::EventType::Shot:            return "Shot";
        case fm::EventType::ShotOnTarget:    return "ShotOnTarget";
        case fm::EventType::Save:            return "Save";
        case fm::EventType::Goal:            return "Goal";
        case fm::EventType::OutTouchline:    return "OutTouchline";
        case fm::EventType::OutGoalLine:     return "OutGoalLine";
        case fm::EventType::Corner:          return "Corner";
        case fm::EventType::GoalKick:        return "GoalKick";
        case fm::EventType::ThrowIn:         return "ThrowIn";
        case fm::EventType::HalfTime:        return "HalfTime";
        case fm::EventType::FullTime:        return "FullTime";
        case fm::EventType::TacticalChange:  return "TacticalChange";
        default:                             return "Event";
    }
}

std::string format_player_label(int8_t slot) {
    if (slot < 0 || slot >= 22) return "Unknown";
    const char* team = (slot < 11) ? "Home" : "Away";
    int jersey = (slot < 11) ? (slot + 1) : (slot - 10);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s #%d", team, jersey);
    return std::string(buf);
}

std::string format_event_message(const fm::MatchEvent& evt) {
    std::string p1 = format_player_label(evt.primary_slot);
    std::string p2 = format_player_label(evt.secondary_slot);

    switch (evt.type) {
        case fm::EventType::Kickoff:
            return "Kickoff taken by " + p1;
        case fm::EventType::Goal:
            return "GOAL! Scored by " + p1;
        case fm::EventType::Shot:
            return "Shot by " + p1;
        case fm::EventType::ShotOnTarget:
            return "Shot on target by " + p1;
        case fm::EventType::Save:
            return "Save by " + p1 + " (denied " + p2 + ")";
        case fm::EventType::TackleWon:
            return "Tackle won by " + p1 + " on " + p2;
        case fm::EventType::PassIntercepted:
            return "Interception by " + p1;
        case fm::EventType::Corner:
            return "Corner kick for " + std::string((evt.primary_slot >= 0 && evt.primary_slot < 11) ? "Home" : "Away");
        case fm::EventType::GoalKick:
            return "Goal kick for " + std::string((evt.primary_slot >= 0 && evt.primary_slot < 11) ? "Home" : "Away");
        case fm::EventType::ThrowIn:
            return "Throw-in for " + std::string((evt.primary_slot >= 0 && evt.primary_slot < 11) ? "Home" : "Away");
        case fm::EventType::HalfTime:
            return "--- HALF-TIME ---";
        case fm::EventType::FullTime:
            return "--- FULL-TIME ---";
        case fm::EventType::Pass:
            return "Pass from " + p1 + " to " + p2;
        case fm::EventType::PassComplete:
            return "Pass completed to " + p2;
        case fm::EventType::Dribble:
            return "Dribble run by " + p1;
        default:
            return std::string(event_type_to_str(evt.type));
    }
}

} // namespace

void MatchEngineBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("ping"), &MatchEngineBridge::ping);
    ClassDB::bind_method(D_METHOD("get_engine_version"), &MatchEngineBridge::get_engine_version);
    ClassDB::bind_method(D_METHOD("initialize_match", "seed"), &MatchEngineBridge::initialize_match);
    ClassDB::bind_method(D_METHOD("simulate_full_match"), &MatchEngineBridge::simulate_full_match);
    ClassDB::bind_method(D_METHOD("run_sandbox_demo"), &MatchEngineBridge::run_sandbox_demo);
    ClassDB::bind_method(D_METHOD("get_frame_data", "tick"), &MatchEngineBridge::get_frame_data);
    ClassDB::bind_method(D_METHOD("get_match_metadata"), &MatchEngineBridge::get_match_metadata);
    ClassDB::bind_method(D_METHOD("get_tick_hud_state", "tick"), &MatchEngineBridge::get_tick_hud_state);
    ClassDB::bind_method(D_METHOD("get_match_events"), &MatchEngineBridge::get_match_events);
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
    simulator_.initialize_match(current_state_, seed);
    recording_.clear();
    checkpoint_mgr_.clear();
}

void MatchEngineBridge::simulate_full_match() {
    recording_.clear();
    recording_.reserve(fm::Pitch::TICKS_FULL_TIME);
    checkpoint_mgr_.clear();

    while (current_state_.tick < fm::Pitch::TICKS_FULL_TIME) {
        if (current_state_.tick % fm::CheckpointManager::CHECKPOINT_INTERVAL == 0) {
            checkpoint_mgr_.save_checkpoint(current_state_);
        }
        simulator_.step_tick(current_state_);
        recording_.add_frame(fm::FrameSnapshot::from_state(current_state_), current_state_);
        if (!simulator_.recent_events().empty()) {
            recording_.add_events(simulator_.recent_events());
        }
    }
}

void MatchEngineBridge::run_sandbox_demo() {
    simulator_.run_sandbox_demo(current_state_, recording_, 1000);
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

Dictionary MatchEngineBridge::get_tick_hud_state(int tick) const {
    Dictionary hud;
    if (tick >= 0 && static_cast<std::size_t>(tick) < recording_.frame_count()) {
        const auto& meta = recording_.get_frame_meta(static_cast<std::size_t>(tick));
        hud["score_home"] = meta.score_home;
        hud["score_away"] = meta.score_away;
        hud["play_state"] = static_cast<int>(meta.play_state);
        hud["play_state_name"] = String(play_state_to_str(meta.play_state));
        hud["half"] = meta.half;
    } else {
        hud["score_home"] = 0;
        hud["score_away"] = 0;
        hud["play_state"] = static_cast<int>(fm::PlayState::Kickoff);
        hud["play_state_name"] = String("Kickoff");
        hud["half"] = 1;
    }
    return hud;
}

Array MatchEngineBridge::get_match_events() const {
    Array arr;
    const auto& evts = recording_.events();
    arr.resize(static_cast<int64_t>(evts.size()));
    for (std::size_t i = 0; i < evts.size(); ++i) {
        const auto& evt = evts[i];
        Dictionary d;
        d["tick"] = static_cast<int>(evt.tick);
        d["type"] = static_cast<int>(evt.type);
        d["type_name"] = String(event_type_to_str(evt.type));
        d["primary_slot"] = static_cast<int>(evt.primary_slot);
        d["secondary_slot"] = static_cast<int>(evt.secondary_slot);
        d["team_id"] = (evt.primary_slot >= 0) ? ((evt.primary_slot < 11) ? 0 : 1) : -1;
        d["xg"] = evt.xg_value;
        d["message"] = String(format_event_message(evt).c_str());
        arr[static_cast<int64_t>(i)] = d;
    }
    return arr;
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
