#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "fm/engine/match_stats.hpp"
#include "fm/model/match_state.hpp"

namespace fm::cli {

struct BatchConfig {
    uint32_t matches{200};
    uint32_t threads{8};
    uint8_t home_rating{12};
    uint8_t away_rating{12};
    FormationId home_formation{FormationId::F_442};
    FormationId away_formation{FormationId::F_442};
    bool mix_formations{false};
    uint64_t base_seed{0xDEADBEEFULL};
    std::string csv_out_path{};
    bool validate_targets{false};
};

struct BatchAggregate {
    uint32_t matches{0};
    double wall_time_ms{0.0};
    double avg_match_ms{0.0};

    uint32_t home_wins{0};
    uint32_t draws{0};
    uint32_t away_wins{0};

    double home_win_pct{0.0};
    double draw_pct{0.0};
    double away_win_pct{0.0};

    double goals_per_match{0.0};
    double home_goals_per_match{0.0};
    double away_goals_per_match{0.0};

    double shots_per_match{0.0};
    double home_shots_per_match{0.0};
    double away_shots_per_match{0.0};

    double shots_on_target_pct{0.0};
    double home_sot_pct{0.0};
    double away_sot_pct{0.0};

    double pass_completion_pct{0.0};
    double home_pass_pct{0.0};
    double away_pass_pct{0.0};
    double passes_per_match{0.0};

    double home_possession_pct{50.0};
    double away_possession_pct{50.0};

    double saves_per_match{0.0};
    double interceptions_per_match{0.0};
    double tackles_per_match{0.0};
    double corners_per_match{0.0};
    double throw_ins_per_match{0.0};
    double goal_kicks_per_match{0.0};
    double xg_per_match{0.0};
};

struct BatchResult {
    std::vector<MatchStats> match_stats{};
    BatchAggregate aggregate{};
};

[[nodiscard]] uint64_t derive_match_seed(uint64_t base_seed, uint32_t match_index) noexcept;
[[nodiscard]] BatchResult run_headless_batch(const BatchConfig& config);
[[nodiscard]] bool write_batch_csv(const std::string& path, const BatchResult& result);
void print_batch_summary(const BatchConfig& config, const BatchResult& result);
[[nodiscard]] bool check_calibration_targets(const BatchConfig& config, const BatchAggregate& agg);

} // namespace fm::cli

