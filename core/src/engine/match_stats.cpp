#include "fm/engine/match_stats.hpp"

namespace fm {

namespace {

int8_t team_from_slot(int8_t slot) noexcept {
    if (slot < 0 || slot >= 22) return -1;
    return (slot < 11) ? 0 : 1;
}

} // namespace

void MatchStats::reset(const MatchState& state, uint64_t match_seed, uint8_t r_home, uint8_t r_away) noexcept {
    seed = match_seed;
    home_rating = r_home;
    away_rating = r_away;
    home_formation = state.home_tactics.formation;
    away_formation = state.away_tactics.formation;
    total_ticks = 0;
    open_play_ticks = 0;
    elapsed_ms = 0.0;
    home = TeamMatchStats{};
    away = TeamMatchStats{};
}

void MatchStats::accumulate_tick(const MatchState& state) noexcept {
    total_ticks++;
    if (state.play_state != PlayState::OpenPlay) {
        return;
    }

    open_play_ticks++;
    int8_t poss_team = team_from_slot(state.ball.owner_slot);
    if (poss_team < 0 && (state.ball.last_touch_team == 0 || state.ball.last_touch_team == 1)) {
        poss_team = state.ball.last_touch_team;
    }
    if (poss_team == 0) {
        home.possession_ticks++;
    } else if (poss_team == 1) {
        away.possession_ticks++;
    }
}

void MatchStats::accumulate_events(const std::vector<MatchEvent>& events) noexcept {
    for (const auto& evt : events) {
        const int8_t tid = team_from_slot(evt.primary_slot);
        if (tid < 0) continue;
        TeamMatchStats& team = (tid == 0) ? home : away;

        switch (evt.type) {
            case EventType::Pass:
                team.passes_attempted++;
                break;
            case EventType::PassComplete:
                team.passes_completed++;
                break;
            case EventType::PassIntercepted:
                team.interceptions++;
                break;
            case EventType::TackleWon:
                team.tackles_won++;
                break;
            case EventType::Dribble:
                team.dribbles++;
                break;
            case EventType::Shot:
                team.shots++;
                team.xg_total += evt.xg_value;
                break;
            case EventType::ShotOnTarget:
                team.shots_on_target++;
                break;
            case EventType::Save:
                team.saves++;
                break;
            case EventType::Corner:
                team.corners++;
                break;
            case EventType::ThrowIn:
                team.throw_ins++;
                break;
            case EventType::GoalKick:
                team.goal_kicks++;
                break;
            default:
                break;
        }
    }
}

void MatchStats::finalize(const MatchState& state) noexcept {
    home.goals = state.score_home;
    away.goals = state.score_away;
}

} // namespace fm

