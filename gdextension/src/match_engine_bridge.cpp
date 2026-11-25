#include "match_engine_bridge.hpp"
#include <godot_cpp/core/class_db.hpp>
#include "fm/engine/version.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/model/tactics.hpp"
#include <algorithm>
#include <chrono>
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
        case fm::EventType::TacticalChange: {
            const char* team = (evt.primary_slot >= 0 && evt.primary_slot < 11) ? "Home" : "Away";
            const auto form = fm::sanitize_formation(static_cast<int>(evt.secondary_slot));
            const std::string form_str(fm::formation_name(form));
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Tactics (%s): %s | Line %.2f | Press %.2f",
                          team, form_str.c_str(), evt.location.x, evt.location.y);
            return std::string(buf);
        }
        default:
            return std::string(event_type_to_str(evt.type));
    }
}

fm::MatchEvent make_tactical_change_event(uint32_t tick, uint8_t team_id, const fm::TacticalConfig& cfg) {
    fm::MatchEvent evt{};
    evt.tick = tick;
    evt.type = fm::EventType::TacticalChange;
    evt.primary_slot = (team_id == 0) ? 0 : 11;
    evt.secondary_slot = static_cast<int8_t>(cfg.formation);
    evt.location = fm::Vec2{cfg.defensive_line, cfg.pressing_intensity};
    evt.xg_value = cfg.tempo;
    return evt;
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
    ClassDB::bind_method(D_METHOD("get_checkpoint_count"), &MatchEngineBridge::get_checkpoint_count);
    ClassDB::bind_method(D_METHOD("get_last_sim_time_ms"), &MatchEngineBridge::get_last_sim_time_ms);
    ClassDB::bind_method(D_METHOD("get_team_tactics", "team_id"), &MatchEngineBridge::get_team_tactics);
    ClassDB::bind_method(D_METHOD("apply_tactical_change", "team_id", "formation_id", "defensive_line", "pressing"), &MatchEngineBridge::apply_tactical_change);
    ClassDB::bind_method(D_METHOD("apply_tactical_change_full", "team_id", "formation_id", "defensive_line", "pressing", "passing_directness", "tempo"), &MatchEngineBridge::apply_tactical_change_full);
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
    pending_home_edit_ = PendingTacticalEdit{};
    pending_away_edit_ = PendingTacticalEdit{};
    last_sim_time_ms_ = 0.0;
    simulator_.initialize_match(current_state_, seed);
    recording_.clear();
    checkpoint_mgr_.clear();
}

void MatchEngineBridge::simulate_full_match() {
    if (current_state_.tick >= fm::Pitch::TICKS_FULL_TIME ||
        current_state_.play_state == fm::PlayState::FullTime) {
        simulator_.initialize_match(current_state_, initial_seed_);
    }
    pending_home_edit_ = PendingTacticalEdit{};
    pending_away_edit_ = PendingTacticalEdit{};

    const auto t0 = std::chrono::steady_clock::now();
    simulator_.simulate_match(current_state_, recording_, checkpoint_mgr_, fm::Pitch::TICKS_FULL_TIME);
    const auto t1 = std::chrono::steady_clock::now();
    last_sim_time_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void MatchEngineBridge::run_sandbox_demo() {
    checkpoint_mgr_.clear();
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
    meta["checkpoint_count"] = static_cast<int>(checkpoint_mgr_.count());
    meta["checkpoint_bytes"] = static_cast<int64_t>(checkpoint_mgr_.memory_bytes());
    meta["last_sim_ms"] = last_sim_time_ms_;
    meta["seed"] = static_cast<int64_t>(initial_seed_);
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
        hud["home_formation"] = static_cast<int>(meta.home_formation);
        hud["away_formation"] = static_cast<int>(meta.away_formation);
        hud["home_formation_name"] = String(fm::formation_name(meta.home_formation).data());
        hud["away_formation_name"] = String(fm::formation_name(meta.away_formation).data());
        hud["home_def_line"] = static_cast<float>(meta.home_def_line_pct) / 100.0f;
        hud["home_pressing"] = static_cast<float>(meta.home_press_pct) / 100.0f;
        hud["away_def_line"] = static_cast<float>(meta.away_def_line_pct) / 100.0f;
        hud["away_pressing"] = static_cast<float>(meta.away_press_pct) / 100.0f;
    } else {
        hud["score_home"] = 0;
        hud["score_away"] = 0;
        hud["play_state"] = static_cast<int>(fm::PlayState::Kickoff);
        hud["play_state_name"] = String("Kickoff");
        hud["half"] = 1;
        hud["home_formation"] = static_cast<int>(current_state_.home_tactics.formation);
        hud["away_formation"] = static_cast<int>(current_state_.away_tactics.formation);
        hud["home_formation_name"] = String(fm::formation_name(current_state_.home_tactics.formation).data());
        hud["away_formation_name"] = String(fm::formation_name(current_state_.away_tactics.formation).data());
        hud["home_def_line"] = current_state_.home_tactics.defensive_line;
        hud["home_pressing"] = current_state_.home_tactics.pressing_intensity;
        hud["away_def_line"] = current_state_.away_tactics.defensive_line;
        hud["away_pressing"] = current_state_.away_tactics.pressing_intensity;
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

int MatchEngineBridge::get_checkpoint_count() const {
    return static_cast<int>(checkpoint_mgr_.count());
}

double MatchEngineBridge::get_last_sim_time_ms() const {
    return last_sim_time_ms_;
}

Dictionary MatchEngineBridge::get_team_tactics(int team_id) const {
    const auto& t = (team_id == 0) ? current_state_.home_tactics : current_state_.away_tactics;
    Dictionary d;
    d["team_id"] = (team_id == 0) ? 0 : 1;
    d["formation_id"] = static_cast<int>(t.formation);
    d["formation_name"] = String(fm::formation_name(t.formation).data());
    d["defensive_line"] = t.defensive_line;
    d["pressing_intensity"] = t.pressing_intensity;
    d["passing_directness"] = t.passing_directness;
    d["tempo"] = t.tempo;
    return d;
}

void MatchEngineBridge::apply_tactical_change(int team_id, int formation_id, float defensive_line, float pressing) {
    const fm::FormationId safe_form = fm::sanitize_formation(formation_id);
    const float safe_line = std::clamp(defensive_line, 0.0f, 1.0f);
    const float safe_press = std::clamp(pressing, 0.0f, 1.0f);

    if (recording_.frame_count() == 0) {
        fm::apply_tactical_change(current_state_, static_cast<uint8_t>(team_id == 0 ? 0 : 1),
                                  safe_form, safe_line, safe_press);
        return;
    }

    auto& edit = (team_id == 0) ? pending_home_edit_ : pending_away_edit_;
    edit.active = true;
    edit.formation = safe_form;
    edit.defensive_line = safe_line;
    edit.pressing_intensity = safe_press;
    edit.has_full_extras = false;
}

void MatchEngineBridge::apply_tactical_change_full(int team_id,
                                                   int formation_id,
                                                   float defensive_line,
                                                   float pressing,
                                                   float passing_directness,
                                                   float tempo) {
    const fm::FormationId safe_form = fm::sanitize_formation(formation_id);
    const float safe_line = std::clamp(defensive_line, 0.0f, 1.0f);
    const float safe_press = std::clamp(pressing, 0.0f, 1.0f);
    const float safe_direct = std::clamp(passing_directness, 0.0f, 1.0f);
    const float safe_tempo = std::clamp(tempo, 0.0f, 1.0f);

    if (recording_.frame_count() == 0) {
        const uint8_t safe_team = (team_id == 0) ? 0 : 1;
        fm::TacticalConfig next = (safe_team == 0) ? current_state_.home_tactics : current_state_.away_tactics;
        if (safe_form != next.formation) {
            next.formation = safe_form;
            next.base_anchors_norm = fm::get_default_tactics(safe_form).base_anchors_norm;
        }
        next.defensive_line = safe_line;
        next.pressing_intensity = safe_press;
        next.passing_directness = safe_direct;
        next.tempo = safe_tempo;
        fm::apply_tactical_change(current_state_, safe_team, next);
        return;
    }

    auto& edit = (team_id == 0) ? pending_home_edit_ : pending_away_edit_;
    edit.active = true;
    edit.formation = safe_form;
    edit.defensive_line = safe_line;
    edit.pressing_intensity = safe_press;
    edit.has_full_extras = true;
    edit.passing_directness = safe_direct;
    edit.tempo = safe_tempo;
}

void MatchEngineBridge::rollback_and_resimulate(int target_tick) {
    if (checkpoint_mgr_.count() == 0) {
        return;
    }

    const uint32_t clamped_tick = static_cast<uint32_t>(
        std::clamp(target_tick, 0, static_cast<int>(fm::Pitch::TICKS_FULL_TIME)));

    const PendingTacticalEdit home_edit = pending_home_edit_;
    const PendingTacticalEdit away_edit = pending_away_edit_;
    pending_home_edit_ = PendingTacticalEdit{};
    pending_away_edit_ = PendingTacticalEdit{};

    const auto t0 = std::chrono::steady_clock::now();

    if (home_edit.active) {
        checkpoint_mgr_.remove_tactical_change_at(clamped_tick, 0);
    }
    if (away_edit.active) {
        checkpoint_mgr_.remove_tactical_change_at(clamped_tick, 1);
    }

    if (!simulator_.rollback_to_tick(current_state_, recording_, checkpoint_mgr_, clamped_tick)) {
        return;
    }

    auto build_tactics_for = [this](uint8_t team_id, const PendingTacticalEdit& edit) {
        fm::TacticalConfig next = (team_id == 0) ? current_state_.home_tactics : current_state_.away_tactics;
        if (edit.formation != next.formation) {
            next.formation = edit.formation;
            next.base_anchors_norm = fm::get_default_tactics(edit.formation).base_anchors_norm;
        }
        next.defensive_line = edit.defensive_line;
        next.pressing_intensity = edit.pressing_intensity;
        if (edit.has_full_extras) {
            next.passing_directness = edit.passing_directness;
            next.tempo = edit.tempo;
        }
        return next;
    };

    fm::TacticalConfig home_cfg{};
    fm::TacticalConfig away_cfg{};
    const fm::TacticalConfig* home_ptr = nullptr;
    const fm::TacticalConfig* away_ptr = nullptr;

    if (home_edit.active) {
        home_cfg = build_tactics_for(0, home_edit);
        home_ptr = &home_cfg;
    }
    if (away_edit.active) {
        away_cfg = build_tactics_for(1, away_edit);
        away_ptr = &away_cfg;
    }

    // Re-apply pre-existing tactical history at clamped_tick for untouched teams & apply pending edits
    std::vector<fm::MatchEvent> tactical_events;
    for (const auto& entry : checkpoint_mgr_.tactical_history()) {
        if (entry.tick == current_state_.tick) {
            if (fm::apply_tactical_change(current_state_, entry.team_id, entry.tactics)) {
                tactical_events.push_back(
                    make_tactical_change_event(current_state_.tick, entry.team_id, entry.tactics));
            }
        }
    }

    auto apply_pending = [&](uint8_t team_id, const fm::TacticalConfig* cfg) {
        if (!cfg) return;
        if (fm::apply_tactical_change(current_state_, team_id, *cfg)) {
            const auto& applied = (team_id == 0) ? current_state_.home_tactics : current_state_.away_tactics;
            checkpoint_mgr_.record_tactical_change(current_state_.tick, team_id, applied);
            tactical_events.push_back(
                make_tactical_change_event(current_state_.tick, team_id, applied));
        }
    };

    apply_pending(0, home_ptr);
    apply_pending(1, away_ptr);

    if (!tactical_events.empty()) {
        recording_.add_events(tactical_events);
    }

    simulator_.resume_simulation(current_state_, recording_, checkpoint_mgr_, fm::Pitch::TICKS_FULL_TIME);

    const auto t1 = std::chrono::steady_clock::now();
    last_sim_time_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

} // namespace godot
