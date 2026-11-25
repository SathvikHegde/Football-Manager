#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include "batch_runner.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/match_stats.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/engine/version.hpp"
#include "fm/model/tactics.hpp"

namespace {

void print_usage(const char* exe_name) {
    std::cout << "Usage:\n"
              << "  " << exe_name << " [options]\n"
              << "  " << exe_name << " <single_seed>              (Legacy single-match mode)\n\n"
              << "Options:\n"
              << "  --matches <N>           Number of matches to simulate (default: 200)\n"
              << "  --threads <T>           Number of worker threads (default: 8)\n"
              << "  --home-rating <1..20>   Home base attribute rating (default: 12)\n"
              << "  --away-rating <1..20>   Away base attribute rating (default: 12)\n"
              << "  --home-formation <F>    442 | 433 | 4231 | 352 | 532 (default: 442)\n"
              << "  --away-formation <F>    442 | 433 | 4231 | 352 | 532 (default: 442)\n"
              << "  --mix-formations        Rotate all 5 formations across the batch\n"
              << "  --seed <uint64>         Base PRNG seed (default: 3735928559)\n"
              << "  --out <path.csv>        Write per-match telemetry CSV report\n"
              << "  --validate              Return exit code 1 if targets fail\n"
              << "  -h, --help              Show this help message\n";
}

bool parse_formation(std::string_view text, fm::FormationId& out) {
    if (text == "442" || text == "4-4-2" || text == "0") { out = fm::FormationId::F_442; return true; }
    if (text == "433" || text == "4-3-3" || text == "1") { out = fm::FormationId::F_433; return true; }
    if (text == "4231" || text == "4-2-3-1" || text == "2") { out = fm::FormationId::F_4231; return true; }
    if (text == "352" || text == "3-5-2" || text == "3") { out = fm::FormationId::F_352; return true; }
    if (text == "532" || text == "5-3-2" || text == "4") { out = fm::FormationId::F_532; return true; }
    return false;
}

bool parse_u64(const char* str, uint64_t& out) {
    if (!str || *str == '\0') return false;
    char* end = nullptr;
    unsigned long long v = std::strtoull(str, &end, 0);
    if (end == str || *end != '\0') return false;
    out = static_cast<uint64_t>(v);
    return true;
}

int run_single_match(uint64_t seed) {
    std::cout << "========================================\n";
    std::cout << "Football Manager Engine Simulation CLI\n";
    std::cout << "Status:  " << fm::engine_status() << "\n";
    std::cout << "Version: " << fm::engine_version() << "\n";
    std::cout << "Seed:    " << seed << "\n";
    std::cout << "========================================\n";

    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchStats stats{};

    const auto start = std::chrono::steady_clock::now();
    sim.initialize_match(state, seed, fm::FormationId::F_442, fm::FormationId::F_433, 12, 12);
    stats.reset(state, seed, 12, 12);
    sim.simulate_headless(state, stats, fm::Pitch::TICKS_FULL_TIME);
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Simulated Ticks:  " << stats.total_ticks << " (" << elapsed_ms << " ms)\n";
    std::cout << "Final Score:      Home " << static_cast<int>(stats.home.goals)
              << " - " << static_cast<int>(stats.away.goals) << " Away\n";
    std::cout << "Shots (On Target):" << stats.total_shots() << " (" << stats.total_shots_on_target()
              << ", " << std::setprecision(1) << stats.shots_on_target_pct() << "%)\n";
    std::cout << "Saves:            " << (stats.home.saves + stats.away.saves) << "\n";
    std::cout << "Passes Completed: " << stats.total_passes_completed() << " / " << stats.total_passes_attempted()
              << " (" << stats.pass_completion_pct() << "%)\n";
    std::cout << "Possession Split: " << stats.home_possession_pct() << "% - " << stats.away_possession_pct() << "%\n";
    std::cout << "Interceptions:    " << (stats.home.interceptions + stats.away.interceptions)
              << " | Tackles Won: " << (stats.home.tackles_won + stats.away.tackles_won) << "\n";
    std::cout << "Set Pieces:       Throw-Ins=" << (stats.home.throw_ins + stats.away.throw_ins)
              << ", Corners=" << (stats.home.corners + stats.away.corners)
              << ", GoalKicks=" << (stats.home.goal_kicks + stats.away.goal_kicks) << "\n";
    std::cout << "========================================\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string_view arg1(argv[1]);
        if (arg1 == "-h" || arg1 == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        uint64_t positional_seed = 0;
        if (!arg1.empty() && arg1[0] != '-' && parse_u64(argv[1], positional_seed)) {
            return run_single_match(positional_seed);
        }
    }

    fm::cli::BatchConfig config{};
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        auto require_val = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing value for " << flag << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--matches") {
            const char* v = require_val("--matches");
            uint64_t n = 0;
            if (!v || !parse_u64(v, n) || n == 0) {
                std::cerr << "Error: Invalid --matches value\n";
                return 2;
            }
            config.matches = static_cast<uint32_t>(n);
        } else if (arg == "--threads") {
            const char* v = require_val("--threads");
            uint64_t t = 0;
            if (!v || !parse_u64(v, t) || t == 0) {
                std::cerr << "Error: Invalid --threads value\n";
                return 2;
            }
            config.threads = static_cast<uint32_t>(t);
        } else if (arg == "--home-rating") {
            const char* v = require_val("--home-rating");
            uint64_t r = 0;
            if (!v || !parse_u64(v, r) || r < 1 || r > 20) {
                std::cerr << "Error: Invalid --home-rating (must be 1..20)\n";
                return 2;
            }
            config.home_rating = static_cast<uint8_t>(r);
        } else if (arg == "--away-rating") {
            const char* v = require_val("--away-rating");
            uint64_t r = 0;
            if (!v || !parse_u64(v, r) || r < 1 || r > 20) {
                std::cerr << "Error: Invalid --away-rating (must be 1..20)\n";
                return 2;
            }
            config.away_rating = static_cast<uint8_t>(r);
        } else if (arg == "--home-formation") {
            const char* v = require_val("--home-formation");
            if (!v || !parse_formation(v, config.home_formation)) {
                std::cerr << "Error: Invalid --home-formation (442, 433, 4231, 352, 532)\n";
                return 2;
            }
        } else if (arg == "--away-formation") {
            const char* v = require_val("--away-formation");
            if (!v || !parse_formation(v, config.away_formation)) {
                std::cerr << "Error: Invalid --away-formation (442, 433, 4231, 352, 532)\n";
                return 2;
            }
        } else if (arg == "--mix-formations") {
            config.mix_formations = true;
        } else if (arg == "--seed") {
            const char* v = require_val("--seed");
            if (!v || !parse_u64(v, config.base_seed)) {
                std::cerr << "Error: Invalid --seed value\n";
                return 2;
            }
        } else if (arg == "--out") {
            const char* v = require_val("--out");
            if (!v) return 2;
            config.csv_out_path = v;
        } else if (arg == "--validate") {
            config.validate_targets = true;
        } else {
            std::cerr << "Error: Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    const auto result = fm::cli::run_headless_batch(config);
    fm::cli::print_batch_summary(config, result);

    if (!config.csv_out_path.empty()) {
        if (!fm::cli::write_batch_csv(config.csv_out_path, result)) {
            std::cerr << "Error: Failed to write CSV report to '" << config.csv_out_path << "'\n";
            return 3;
        }
        std::cout << "CSV telemetry written to: " << config.csv_out_path << "\n";
    }

    if (config.validate_targets && !fm::cli::check_calibration_targets(config, result.aggregate)) {
        std::cerr << "[VALIDATION FAILED] Aggregate batch metrics outside target calibration bands.\n";
        return 1;
    }

    return 0;
}
