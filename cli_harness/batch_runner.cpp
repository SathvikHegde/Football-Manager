#include "batch_runner.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/engine/version.hpp"
#include "fm/model/tactics.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace fm::cli {

namespace {

constexpr std::array<FormationId, 5> ALL_FORMATIONS = {
    FormationId::F_442,
    FormationId::F_433,
    FormationId::F_4231,
    FormationId::F_352,
    FormationId::F_532
};

BatchAggregate compute_aggregate(const std::vector<MatchStats>& stats, double wall_ms) {
    BatchAggregate agg{};
    agg.matches = static_cast<uint32_t>(stats.size());
    agg.wall_time_ms = wall_ms;
    if (stats.empty()) {
        return agg;
    }

    uint64_t sum_home_goals = 0;
    uint64_t sum_away_goals = 0;
    uint64_t sum_home_shots = 0;
    uint64_t sum_away_shots = 0;
    uint64_t sum_home_sot = 0;
    uint64_t sum_away_sot = 0;
    uint64_t sum_home_pass_att = 0;
    uint64_t sum_away_pass_att = 0;
    uint64_t sum_home_pass_cmp = 0;
    uint64_t sum_away_pass_cmp = 0;
    uint64_t sum_saves = 0;
    uint64_t sum_interceptions = 0;
    uint64_t sum_tackles = 0;
    uint64_t sum_corners = 0;
    uint64_t sum_throw_ins = 0;
    uint64_t sum_goal_kicks = 0;
    uint64_t sum_home_poss = 0;
    uint64_t sum_away_poss = 0;
    double sum_xg = 0.0;
    double sum_match_ms = 0.0;

    for (const auto& m : stats) {
        if (m.home.goals > m.away.goals) agg.home_wins++;
        else if (m.away.goals > m.home.goals) agg.away_wins++;
        else agg.draws++;

        sum_home_goals += m.home.goals;
        sum_away_goals += m.away.goals;
        sum_home_shots += m.home.shots;
        sum_away_shots += m.away.shots;
        sum_home_sot += m.home.shots_on_target;
        sum_away_sot += m.away.shots_on_target;
        sum_home_pass_att += m.home.passes_attempted;
        sum_away_pass_att += m.away.passes_attempted;
        sum_home_pass_cmp += m.home.passes_completed;
        sum_away_pass_cmp += m.away.passes_completed;
        sum_saves += static_cast<uint64_t>(m.home.saves) + m.away.saves;
        sum_interceptions += static_cast<uint64_t>(m.home.interceptions) + m.away.interceptions;
        sum_tackles += static_cast<uint64_t>(m.home.tackles_won) + m.away.tackles_won;
        sum_corners += static_cast<uint64_t>(m.home.corners) + m.away.corners;
        sum_throw_ins += static_cast<uint64_t>(m.home.throw_ins) + m.away.throw_ins;
        sum_goal_kicks += static_cast<uint64_t>(m.home.goal_kicks) + m.away.goal_kicks;
        sum_home_poss += m.home.possession_ticks;
        sum_away_poss += m.away.possession_ticks;
        sum_xg += static_cast<double>(m.home.xg_total) + static_cast<double>(m.away.xg_total);
        sum_match_ms += m.elapsed_ms;
    }

    const double n = static_cast<double>(stats.size());
    agg.avg_match_ms = sum_match_ms / n;
    agg.home_win_pct = 100.0 * static_cast<double>(agg.home_wins) / n;
    agg.draw_pct = 100.0 * static_cast<double>(agg.draws) / n;
    agg.away_win_pct = 100.0 * static_cast<double>(agg.away_wins) / n;

    agg.home_goals_per_match = static_cast<double>(sum_home_goals) / n;
    agg.away_goals_per_match = static_cast<double>(sum_away_goals) / n;
    agg.goals_per_match = agg.home_goals_per_match + agg.away_goals_per_match;

    agg.home_shots_per_match = static_cast<double>(sum_home_shots) / n;
    agg.away_shots_per_match = static_cast<double>(sum_away_shots) / n;
    agg.shots_per_match = agg.home_shots_per_match + agg.away_shots_per_match;

    const uint64_t total_shots = sum_home_shots + sum_away_shots;
    const uint64_t total_sot = sum_home_sot + sum_away_sot;
    agg.shots_on_target_pct = (total_shots > 0) ? (100.0 * static_cast<double>(total_sot) / static_cast<double>(total_shots)) : 0.0;
    agg.home_sot_pct = (sum_home_shots > 0) ? (100.0 * static_cast<double>(sum_home_sot) / static_cast<double>(sum_home_shots)) : 0.0;
    agg.away_sot_pct = (sum_away_shots > 0) ? (100.0 * static_cast<double>(sum_away_sot) / static_cast<double>(sum_away_shots)) : 0.0;

    const uint64_t total_pass_att = sum_home_pass_att + sum_away_pass_att;
    const uint64_t total_pass_cmp = sum_home_pass_cmp + sum_away_pass_cmp;
    agg.passes_per_match = static_cast<double>(total_pass_att) / n;
    agg.pass_completion_pct = (total_pass_att > 0) ? (100.0 * static_cast<double>(total_pass_cmp) / static_cast<double>(total_pass_att)) : 0.0;
    agg.home_pass_pct = (sum_home_pass_att > 0) ? (100.0 * static_cast<double>(sum_home_pass_cmp) / static_cast<double>(sum_home_pass_att)) : 0.0;
    agg.away_pass_pct = (sum_away_pass_att > 0) ? (100.0 * static_cast<double>(sum_away_pass_cmp) / static_cast<double>(sum_away_pass_att)) : 0.0;

    const uint64_t total_poss = sum_home_poss + sum_away_poss;
    agg.home_possession_pct = (total_poss > 0) ? (100.0 * static_cast<double>(sum_home_poss) / static_cast<double>(total_poss)) : 50.0;
    agg.away_possession_pct = (total_poss > 0) ? (100.0 * static_cast<double>(sum_away_poss) / static_cast<double>(total_poss)) : 50.0;

    agg.saves_per_match = static_cast<double>(sum_saves) / n;
    agg.interceptions_per_match = static_cast<double>(sum_interceptions) / n;
    agg.tackles_per_match = static_cast<double>(sum_tackles) / n;
    agg.corners_per_match = static_cast<double>(sum_corners) / n;
    agg.throw_ins_per_match = static_cast<double>(sum_throw_ins) / n;
    agg.goal_kicks_per_match = static_cast<double>(sum_goal_kicks) / n;
    agg.xg_per_match = sum_xg / n;

    return agg;
}

const char* pass_tag(bool ok) {
    return ok ? "PASS" : "FAIL";
}

std::string fmt_pct(double val) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << val << '%';
    return oss.str();
}

} // namespace

uint64_t derive_match_seed(uint64_t base_seed, uint32_t match_index) noexcept {
    // SplitMix64 mixing so sequential match indices yield independent PRNG streams
    uint64_t z = base_seed + (static_cast<uint64_t>(match_index) + 1ULL) * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31u);
    return (z == 0ULL) ? 0x853c49e6748fea9bULL : z;
}

BatchResult run_headless_batch(const BatchConfig& config) {
    BatchResult result;
    const uint32_t count = std::max<uint32_t>(1u, config.matches);
    const uint32_t worker_count = std::clamp<uint32_t>(config.threads, 1u, count);

    result.match_stats.resize(count);
    std::atomic<uint32_t> next_index{0u};

    const auto batch_start = std::chrono::steady_clock::now();

    auto worker_fn = [&]() {
        MatchSimulator sim;
        MatchState state{};

        while (true) {
            const uint32_t idx = next_index.fetch_add(1u, std::memory_order_relaxed);
            if (idx >= count) {
                break;
            }

            const uint64_t seed = derive_match_seed(config.base_seed, idx);
            const FormationId home_form = config.mix_formations
                ? ALL_FORMATIONS[idx % ALL_FORMATIONS.size()]
                : config.home_formation;
            const FormationId away_form = config.mix_formations
                ? ALL_FORMATIONS[(idx / ALL_FORMATIONS.size() + 1u) % ALL_FORMATIONS.size()]
                : config.away_formation;

            MatchStats& out_stats = result.match_stats[idx];
            const auto t0 = std::chrono::steady_clock::now();
            sim.initialize_match(state, seed, home_form, away_form, config.home_rating, config.away_rating);
            out_stats.reset(state, seed, config.home_rating, config.away_rating);
            sim.simulate_headless(state, out_stats, Pitch::TICKS_FULL_TIME);
            const auto t1 = std::chrono::steady_clock::now();

            out_stats.seed = seed;
            out_stats.home_rating = config.home_rating;
            out_stats.away_rating = config.away_rating;
            out_stats.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (uint32_t w = 0; w < worker_count; ++w) {
        workers.emplace_back(worker_fn);
    }
    for (auto& th : workers) {
        th.join();
    }

    const auto batch_end = std::chrono::steady_clock::now();
    const double wall_ms = std::chrono::duration<double, std::milli>(batch_end - batch_start).count();
    result.aggregate = compute_aggregate(result.match_stats, wall_ms);
    return result;
}

bool write_batch_csv(const std::string& path, const BatchResult& result) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "match_index,seed,home_rating,away_rating,home_formation,away_formation,"
        << "home_goals,away_goals,total_goals,"
        << "home_shots,away_shots,total_shots,"
        << "home_sot,away_sot,total_sot,sot_pct,"
        << "home_saves,away_saves,"
        << "home_passes_att,home_passes_cmp,away_passes_att,away_passes_cmp,"
        << "pass_completion_pct,home_pass_pct,away_pass_pct,"
        << "home_interceptions,away_interceptions,home_tackles,away_tackles,"
        << "home_corners,away_corners,home_throw_ins,away_throw_ins,home_goal_kicks,away_goal_kicks,"
        << "home_possession_pct,away_possession_pct,home_xg,away_xg,sim_ms\n";

    out << std::fixed << std::setprecision(2);
    for (std::size_t i = 0; i < result.match_stats.size(); ++i) {
        const auto& m = result.match_stats[i];
        out << i << ','
            << m.seed << ','
            << static_cast<int>(m.home_rating) << ','
            << static_cast<int>(m.away_rating) << ','
            << formation_name(m.home_formation) << ','
            << formation_name(m.away_formation) << ','
            << m.home.goals << ','
            << m.away.goals << ','
            << m.total_goals() << ','
            << m.home.shots << ','
            << m.away.shots << ','
            << m.total_shots() << ','
            << m.home.shots_on_target << ','
            << m.away.shots_on_target << ','
            << m.total_shots_on_target() << ','
            << m.shots_on_target_pct() << ','
            << m.home.saves << ','
            << m.away.saves << ','
            << m.home.passes_attempted << ','
            << m.home.passes_completed << ','
            << m.away.passes_attempted << ','
            << m.away.passes_completed << ','
            << m.pass_completion_pct() << ','
            << m.home.pass_completion_pct() << ','
            << m.away.pass_completion_pct() << ','
            << m.home.interceptions << ','
            << m.away.interceptions << ','
            << m.home.tackles_won << ','
            << m.away.tackles_won << ','
            << m.home.corners << ','
            << m.away.corners << ','
            << m.home.throw_ins << ','
            << m.away.throw_ins << ','
            << m.home.goal_kicks << ','
            << m.away.goal_kicks << ','
            << m.home_possession_pct() << ','
            << m.away_possession_pct() << ','
            << m.home.xg_total << ','
            << m.away.xg_total << ','
            << m.elapsed_ms << '\n';
    }

    return out.good();
}

bool check_calibration_targets(const BatchConfig& config, const BatchAggregate& agg) {
    const int diff = static_cast<int>(config.home_rating) - static_cast<int>(config.away_rating);
    if (diff == 0) {
        const bool ok_goals = (agg.goals_per_match >= 2.4 && agg.goals_per_match <= 3.0);
        const bool ok_shots = (agg.shots_per_match >= 20.0 && agg.shots_per_match <= 28.0);
        const bool ok_sot   = (agg.shots_on_target_pct >= 30.0 && agg.shots_on_target_pct <= 42.0);
        const bool ok_pass  = (agg.pass_completion_pct >= 78.0 && agg.pass_completion_pct <= 86.0);
        const bool ok_poss  = (agg.home_possession_pct >= 48.0 && agg.home_possession_pct <= 52.0);
        return ok_goals && ok_shots && ok_sot && ok_pass && ok_poss;
    }

    if (std::abs(diff) == 4) {
        const double fav_win_pct  = (diff > 0) ? agg.home_win_pct : agg.away_win_pct;
        const double fav_poss_pct = (diff > 0) ? agg.home_possession_pct : agg.away_possession_pct;
        const bool ok_goals = (agg.goals_per_match >= 3.2 && agg.goals_per_match <= 4.1);
        const bool ok_win   = (fav_win_pct > 65.0);
        const bool ok_shots = (agg.shots_per_match >= 24.0 && agg.shots_per_match <= 32.0);
        const bool ok_sot   = (agg.shots_on_target_pct >= 35.0 && agg.shots_on_target_pct <= 48.0);
        const bool ok_pass  = (agg.pass_completion_pct >= 82.0 && agg.pass_completion_pct <= 89.0);
        const bool ok_poss  = (fav_poss_pct >= 58.0 && fav_poss_pct <= 66.0);
        return ok_goals && ok_win && ok_shots && ok_sot && ok_pass && ok_poss;
    }

    return true;
}

void print_batch_summary(const BatchConfig& config, const BatchResult& result) {
    const auto& agg = result.aggregate;
    const double wall_sec = agg.wall_time_ms / 1000.0;
    const double throughput = (wall_sec > 1e-6) ? (static_cast<double>(agg.matches) / wall_sec) : 0.0;
    const int diff = static_cast<int>(config.home_rating) - static_cast<int>(config.away_rating);
    const bool is_equal = (diff == 0);
    const bool is_lead4 = (std::abs(diff) == 4);

    std::cout << "\n========================================================================\n";
    std::cout << " Football Manager Headless Calibration Batch (" << fm::engine_version() << ")\n";
    std::cout << "========================================================================\n";
    std::cout << " Matches:        " << agg.matches
              << "  | Threads: " << std::clamp<uint32_t>(config.threads, 1u, std::max<uint32_t>(1u, config.matches))
              << "  | Base Seed: " << config.base_seed << "\n";
    std::cout << " Home Team:      Rating " << static_cast<int>(config.home_rating)
              << " (" << (config.mix_formations ? "Mixed" : formation_name(config.home_formation)) << ")\n";
    std::cout << " Away Team:      Rating " << static_cast<int>(config.away_rating)
              << " (" << (config.mix_formations ? "Mixed" : formation_name(config.away_formation)) << ")\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << " Wall Time:      " << wall_sec << " s (" << std::setprecision(1) << throughput
              << " matches/sec, avg " << std::setprecision(2) << agg.avg_match_ms << " ms/match)\n";
    std::cout << " Outcomes (W/D/L): " << agg.home_wins << " / " << agg.draws << " / " << agg.away_wins
              << "  (" << std::setprecision(1) << agg.home_win_pct << "% / "
              << agg.draw_pct << "% / " << agg.away_win_pct << "%)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left
              << std::setw(24) << " Metric"
              << std::setw(14) << "Observed"
              << std::setw(24) << "Target Band"
              << "Status\n";
    std::cout << "------------------------------------------------------------------------\n";

    if (is_equal) {
        std::cout << std::setw(24) << " Goals / Match"
                  << std::setw(14) << std::setprecision(2) << agg.goals_per_match
                  << std::setw(24) << "2.40 - 3.00"
                  << pass_tag(agg.goals_per_match >= 2.4 && agg.goals_per_match <= 3.0) << "\n";
        std::cout << std::setw(24) << " Shots / Match"
                  << std::setw(14) << std::setprecision(2) << agg.shots_per_match
                  << std::setw(24) << "20.00 - 28.00"
                  << pass_tag(agg.shots_per_match >= 20.0 && agg.shots_per_match <= 28.0) << "\n";
        std::cout << std::setw(24) << " Shots on Target %"
                  << std::setw(14) << fmt_pct(agg.shots_on_target_pct)
                  << std::setw(24) << "30.0% - 42.0%"
                  << pass_tag(agg.shots_on_target_pct >= 30.0 && agg.shots_on_target_pct <= 42.0) << "\n";
        std::cout << std::setw(24) << " Pass Completion %"
                  << std::setw(14) << fmt_pct(agg.pass_completion_pct)
                  << std::setw(24) << "78.0% - 86.0%"
                  << pass_tag(agg.pass_completion_pct >= 78.0 && agg.pass_completion_pct <= 86.0) << "\n";
        std::cout << std::setw(24) << " Home Possession %"
                  << std::setw(14) << fmt_pct(agg.home_possession_pct)
                  << std::setw(24) << "48.0% - 52.0%"
                  << pass_tag(agg.home_possession_pct >= 48.0 && agg.home_possession_pct <= 52.0) << "\n";
    } else if (is_lead4) {
        const double fav_win  = (diff > 0) ? agg.home_win_pct : agg.away_win_pct;
        const double fav_poss = (diff > 0) ? agg.home_possession_pct : agg.away_possession_pct;
        std::cout << std::setw(24) << " Goals / Match"
                  << std::setw(14) << std::setprecision(2) << agg.goals_per_match
                  << std::setw(24) << "3.20 - 4.10"
                  << pass_tag(agg.goals_per_match >= 3.2 && agg.goals_per_match <= 4.1) << "\n";
        std::cout << std::setw(24) << " Favorite Win %"
                  << std::setw(14) << fmt_pct(fav_win)
                  << std::setw(24) << "> 65.0%"
                  << pass_tag(fav_win > 65.0) << "\n";
        std::cout << std::setw(24) << " Shots / Match"
                  << std::setw(14) << std::setprecision(2) << agg.shots_per_match
                  << std::setw(24) << "24.00 - 32.00"
                  << pass_tag(agg.shots_per_match >= 24.0 && agg.shots_per_match <= 32.0) << "\n";
        std::cout << std::setw(24) << " Shots on Target %"
                  << std::setw(14) << fmt_pct(agg.shots_on_target_pct)
                  << std::setw(24) << "35.0% - 48.0%"
                  << pass_tag(agg.shots_on_target_pct >= 35.0 && agg.shots_on_target_pct <= 48.0) << "\n";
        std::cout << std::setw(24) << " Pass Completion %"
                  << std::setw(14) << fmt_pct(agg.pass_completion_pct)
                  << std::setw(24) << "82.0% - 89.0%"
                  << pass_tag(agg.pass_completion_pct >= 82.0 && agg.pass_completion_pct <= 89.0) << "\n";
        std::cout << std::setw(24) << " Favorite Poss %"
                  << std::setw(14) << fmt_pct(fav_poss)
                  << std::setw(24) << "58.0% - 66.0%"
                  << pass_tag(fav_poss >= 58.0 && fav_poss <= 66.0) << "\n";
    } else {
        std::cout << std::setw(24) << " Goals / Match"
                  << std::setw(14) << std::setprecision(2) << agg.goals_per_match << "N/A\n";
        std::cout << std::setw(24) << " Shots / Match"
                  << std::setw(14) << std::setprecision(2) << agg.shots_per_match << "N/A\n";
        std::cout << std::setw(24) << " Shots on Target %"
                  << std::setw(14) << std::setprecision(1) << agg.shots_on_target_pct << "N/A\n";
        std::cout << std::setw(24) << " Pass Completion %"
                  << std::setw(14) << std::setprecision(1) << agg.pass_completion_pct << "N/A\n";
        std::cout << std::setw(24) << " Home Possession %"
                  << std::setw(14) << std::setprecision(1) << agg.home_possession_pct << "N/A\n";
    }

    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::setprecision(2);
    std::cout << " Home/Away Goals:   " << agg.home_goals_per_match << " - " << agg.away_goals_per_match
              << "  | Home/Away Shots: " << agg.home_shots_per_match << " - " << agg.away_shots_per_match << "\n";
    std::cout << std::setprecision(1);
    std::cout << " Home/Away SoT %:   " << agg.home_sot_pct << "% / " << agg.away_sot_pct
              << "% | Home/Away Pass %: " << agg.home_pass_pct << "% / " << agg.away_pass_pct << "%\n";
    std::cout << " Passes/Match:      " << agg.passes_per_match
              << " | Saves/Match: " << agg.saves_per_match
              << " | xG/Match: " << std::setprecision(2) << agg.xg_per_match << "\n";
    std::cout << std::setprecision(1);
    std::cout << " Interceptions:     " << agg.interceptions_per_match
              << " | Tackles: " << agg.tackles_per_match
              << " | Corners: " << agg.corners_per_match
              << " | Throws: " << agg.throw_ins_per_match
              << " | GK: " << agg.goal_kicks_per_match << "\n";
    std::cout << "========================================================================\n";
}

} // namespace fm::cli

