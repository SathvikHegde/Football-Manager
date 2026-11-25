#pragma once
#include <cstdint>
#include <vector>
#include "fm/model/match_event.hpp"
#include "fm/model/match_state.hpp"

namespace fm {

struct TeamMatchStats {
    uint16_t goals{0};
    uint16_t shots{0};
    uint16_t shots_on_target{0};
    uint16_t saves{0};
    uint16_t passes_attempted{0};
    uint16_t passes_completed{0};
    uint16_t interceptions{0};
    uint16_t tackles_won{0};
    uint16_t dribbles{0};
    uint16_t corners{0};
    uint16_t throw_ins{0};
    uint16_t goal_kicks{0};
    uint32_t possession_ticks{0};
    float xg_total{0.0f};

    [[nodiscard]] double shot_accuracy_pct() const noexcept {
        return (shots > 0) ? (100.0 * static_cast<double>(shots_on_target) / static_cast<double>(shots)) : 0.0;
    }

    [[nodiscard]] double pass_completion_pct() const noexcept {
        return (passes_attempted > 0)
            ? (100.0 * static_cast<double>(passes_completed) / static_cast<double>(passes_attempted))
            : 0.0;
    }
};

struct MatchStats {
    uint64_t seed{0};
    uint8_t home_rating{12};
    uint8_t away_rating{12};
    FormationId home_formation{FormationId::F_442};
    FormationId away_formation{FormationId::F_442};
    uint32_t total_ticks{0};
    uint32_t open_play_ticks{0};
    double elapsed_ms{0.0};
    TeamMatchStats home{};
    TeamMatchStats away{};

    void reset(const MatchState& state, uint64_t match_seed, uint8_t r_home, uint8_t r_away) noexcept;
    void accumulate_tick(const MatchState& state) noexcept;
    void accumulate_events(const std::vector<MatchEvent>& events) noexcept;
    void finalize(const MatchState& state) noexcept;

    [[nodiscard]] uint32_t total_goals() const noexcept {
        return static_cast<uint32_t>(home.goals) + static_cast<uint32_t>(away.goals);
    }

    [[nodiscard]] uint32_t total_shots() const noexcept {
        return static_cast<uint32_t>(home.shots) + static_cast<uint32_t>(away.shots);
    }

    [[nodiscard]] uint32_t total_shots_on_target() const noexcept {
        return static_cast<uint32_t>(home.shots_on_target) + static_cast<uint32_t>(away.shots_on_target);
    }

    [[nodiscard]] uint32_t total_passes_attempted() const noexcept {
        return static_cast<uint32_t>(home.passes_attempted) + static_cast<uint32_t>(away.passes_attempted);
    }

    [[nodiscard]] uint32_t total_passes_completed() const noexcept {
        return static_cast<uint32_t>(home.passes_completed) + static_cast<uint32_t>(away.passes_completed);
    }

    [[nodiscard]] double shots_on_target_pct() const noexcept {
        const uint32_t shots = total_shots();
        return (shots > 0) ? (100.0 * static_cast<double>(total_shots_on_target()) / static_cast<double>(shots)) : 0.0;
    }

    [[nodiscard]] double pass_completion_pct() const noexcept {
        const uint32_t att = total_passes_attempted();
        return (att > 0) ? (100.0 * static_cast<double>(total_passes_completed()) / static_cast<double>(att)) : 0.0;
    }

    [[nodiscard]] double home_possession_pct() const noexcept {
        const uint32_t total_poss = home.possession_ticks + away.possession_ticks;
        return (total_poss > 0) ? (100.0 * static_cast<double>(home.possession_ticks) / static_cast<double>(total_poss)) : 50.0;
    }

    [[nodiscard]] double away_possession_pct() const noexcept {
        const uint32_t total_poss = home.possession_ticks + away.possession_ticks;
        return (total_poss > 0) ? (100.0 * static_cast<double>(away.possession_ticks) / static_cast<double>(total_poss)) : 50.0;
    }
};

} // namespace fm

