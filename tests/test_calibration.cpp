#include "test_framework.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/match_stats.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/model/team.hpp"
#include "fm/recording/match_recording.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace {

uint64_t splitmix_seed(uint64_t base_seed, uint32_t idx) noexcept {
    uint64_t z = base_seed + (static_cast<uint64_t>(idx) + 1ULL) * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31u);
    return (z == 0ULL) ? 0x853c49e6748fea9bULL : z;
}

struct BatchSummary {
    uint32_t matches{0};
    double wall_ms{0.0};
    uint32_t home_wins{0};
    uint32_t draws{0};
    uint32_t away_wins{0};
    double goals_per_match{0.0};
    double shots_per_match{0.0};
    double sot_pct{0.0};
    double pass_pct{0.0};
    double home_poss_pct{50.0};
    double home_win_pct{0.0};
};

std::vector<fm::MatchStats> run_parallel_matches(uint32_t count,
                                                 uint32_t thread_count,
                                                 uint8_t home_rating,
                                                 uint8_t away_rating,
                                                 uint64_t base_seed) {
    std::vector<fm::MatchStats> results(count);
    std::atomic<uint32_t> next_idx{0u};
    const uint32_t workers_n = std::clamp<uint32_t>(thread_count, 1u, count);

    auto worker = [&]() {
        fm::MatchSimulator sim;
        fm::MatchState state{};
        while (true) {
            const uint32_t idx = next_idx.fetch_add(1u, std::memory_order_relaxed);
            if (idx >= count) break;
            const uint64_t seed = splitmix_seed(base_seed, idx);
            sim.initialize_match(state, seed, fm::FormationId::F_442, fm::FormationId::F_442, home_rating, away_rating);
            results[idx].reset(state, seed, home_rating, away_rating);
            sim.simulate_headless(state, results[idx], fm::Pitch::TICKS_FULL_TIME);
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(workers_n);
    for (uint32_t t = 0; t < workers_n; ++t) {
        workers.emplace_back(worker);
    }
    for (auto& th : workers) {
        th.join();
    }
    return results;
}

BatchSummary summarize_batch(const std::vector<fm::MatchStats>& matches, double wall_ms) {
    BatchSummary s{};
    s.matches = static_cast<uint32_t>(matches.size());
    s.wall_ms = wall_ms;

    uint64_t goals = 0;
    uint64_t shots = 0;
    uint64_t sot = 0;
    uint64_t pass_att = 0;
    uint64_t pass_cmp = 0;
    uint64_t home_poss = 0;
    uint64_t away_poss = 0;

    for (const auto& m : matches) {
        if (m.home.goals > m.away.goals) s.home_wins++;
        else if (m.away.goals > m.home.goals) s.away_wins++;
        else s.draws++;

        goals += m.total_goals();
        shots += m.total_shots();
        sot += m.total_shots_on_target();
        pass_att += m.total_passes_attempted();
        pass_cmp += m.total_passes_completed();
        home_poss += m.home.possession_ticks;
        away_poss += m.away.possession_ticks;
    }

    const double n = static_cast<double>(matches.size());
    s.goals_per_match = static_cast<double>(goals) / n;
    s.shots_per_match = static_cast<double>(shots) / n;
    s.sot_pct = (shots > 0) ? (100.0 * static_cast<double>(sot) / static_cast<double>(shots)) : 0.0;
    s.pass_pct = (pass_att > 0) ? (100.0 * static_cast<double>(pass_cmp) / static_cast<double>(pass_att)) : 0.0;
    const uint64_t total_poss = home_poss + away_poss;
    s.home_poss_pct = (total_poss > 0) ? (100.0 * static_cast<double>(home_poss) / static_cast<double>(total_poss)) : 50.0;
    s.home_win_pct = 100.0 * static_cast<double>(s.home_wins) / n;
    return s;
}

} // namespace

TEST_CASE("Team Rating Profiles Preserve Role Specialization and Skill Monotonicity", "[calibration]") {
    const auto gk12 = fm::make_role_attributes(fm::RoleId::Goalkeeper, 12);
    const auto fwd12 = fm::make_role_attributes(fm::RoleId::Forward, 12);
    const auto mid12 = fm::make_role_attributes(fm::RoleId::Midfielder, 12);
    const auto fwd16 = fm::make_role_attributes(fm::RoleId::Forward, 16);

    REQUIRE(gk12.positioning > gk12.shooting);
    REQUIRE(fwd12.shooting > fwd12.tackling);
    REQUIRE(mid12.passing > mid12.shooting);
    REQUIRE(fwd16.shooting == static_cast<uint8_t>(fwd12.shooting + 4));
    REQUIRE(fwd16.passing == static_cast<uint8_t>(fwd12.passing + 4));
    REQUIRE(fwd16.decisions == static_cast<uint8_t>(fwd12.decisions + 4));

    fm::MatchState state{};
    fm::MatchSimulator sim;
    sim.initialize_match(state, 42ULL, fm::FormationId::F_442, fm::FormationId::F_433, 16, 12);
    REQUIRE(state.players[9].attrs.shooting > state.players[20].attrs.shooting);
    REQUIRE(state.players[6].attrs.passing > state.players[17].attrs.passing);
}

TEST_CASE("Headless Simulation Matches Recorded Simulation Bit-Identically", "[calibration]") {
    fm::MatchSimulator sim1;
    fm::MatchSimulator sim2;
    fm::MatchState state_headless{};
    fm::MatchState state_recorded{};
    fm::MatchStats stats{};
    fm::MatchRecording recording;

    sim1.initialize_match(state_headless, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433, 14, 10);
    sim2.initialize_match(state_recorded, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433, 14, 10);

    stats.reset(state_headless, 0xDEADBEEFULL, 14, 10);
    sim1.simulate_headless(state_headless, stats, fm::Pitch::TICKS_FULL_TIME);
    sim2.simulate_match(state_recorded, recording, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(state_headless == state_recorded);
    REQUIRE(stats.total_ticks == recording.frame_count());
    REQUIRE(stats.home.goals == state_recorded.score_home);
    REQUIRE(stats.away.goals == state_recorded.score_away);

    uint32_t rec_shots = 0;
    uint32_t rec_sot = 0;
    uint32_t rec_pass_att = 0;
    uint32_t rec_pass_cmp = 0;
    uint32_t rec_saves = 0;
    for (const auto& evt : recording.events()) {
        if (evt.type == fm::EventType::Shot) rec_shots++;
        else if (evt.type == fm::EventType::ShotOnTarget) rec_sot++;
        else if (evt.type == fm::EventType::Pass) rec_pass_att++;
        else if (evt.type == fm::EventType::PassComplete) rec_pass_cmp++;
        else if (evt.type == fm::EventType::Save) rec_saves++;
    }

    REQUIRE(stats.total_shots() == rec_shots);
    REQUIRE(stats.total_shots_on_target() == rec_sot);
    REQUIRE(stats.total_passes_attempted() == rec_pass_att);
    REQUIRE(stats.total_passes_completed() == rec_pass_cmp);
    REQUIRE(static_cast<uint32_t>(stats.home.saves + stats.away.saves) == rec_saves);
}

TEST_CASE("Multi-Threaded Headless Batch Is Thread-Count Independent and Deterministic", "[calibration]") {
    const auto single_thread = run_parallel_matches(8, 1, 12, 12, 77777ULL);
    const auto eight_threads = run_parallel_matches(8, 8, 12, 12, 77777ULL);

    REQUIRE(single_thread.size() == 8);
    REQUIRE(eight_threads.size() == 8);

    for (std::size_t i = 0; i < single_thread.size(); ++i) {
        const auto& a = single_thread[i];
        const auto& b = eight_threads[i];
        REQUIRE(a.seed == b.seed);
        REQUIRE(a.home.goals == b.home.goals);
        REQUIRE(a.away.goals == b.away.goals);
        REQUIRE(a.home.shots == b.home.shots);
        REQUIRE(a.away.shots == b.away.shots);
        REQUIRE(a.home.passes_completed == b.home.passes_completed);
        REQUIRE(a.away.passes_completed == b.away.passes_completed);
        REQUIRE(a.home.possession_ticks == b.home.possession_ticks);
        REQUIRE(a.away.possession_ticks == b.away.possession_ticks);
    }
}

#ifdef NDEBUG
TEST_CASE("Stage 4 DoD - 200-Match Equal & +4 Lead Statistical Calibration (<30s on 8 Threads)", "[calibration]") {
    const auto t0 = std::chrono::steady_clock::now();
    const auto equal_matches = run_parallel_matches(200, 8, 12, 12, 0xDEADBEEFULL);
    const auto t1 = std::chrono::steady_clock::now();
    const double equal_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const auto eq = summarize_batch(equal_matches, equal_ms);

    REQUIRE(eq.wall_ms < 30000.0);
    REQUIRE(eq.goals_per_match >= 2.4);
    REQUIRE(eq.goals_per_match <= 3.0);
    REQUIRE(eq.shots_per_match >= 20.0);
    REQUIRE(eq.shots_per_match <= 28.0);
    REQUIRE(eq.sot_pct >= 30.0);
    REQUIRE(eq.sot_pct <= 42.0);
    REQUIRE(eq.pass_pct >= 78.0);
    REQUIRE(eq.pass_pct <= 86.0);
    REQUIRE(eq.home_poss_pct >= 48.0);
    REQUIRE(eq.home_poss_pct <= 52.0);

    const auto t2 = std::chrono::steady_clock::now();
    const auto lead_matches = run_parallel_matches(200, 8, 16, 12, 0xDEADBEEFULL);
    const auto t3 = std::chrono::steady_clock::now();
    const double lead_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    const auto lead = summarize_batch(lead_matches, lead_ms);

    REQUIRE(lead.wall_ms < 30000.0);
    REQUIRE(lead.goals_per_match >= 3.2);
    REQUIRE(lead.goals_per_match <= 4.1);
    REQUIRE(lead.home_win_pct > 65.0);
    REQUIRE(lead.shots_per_match >= 24.0);
    REQUIRE(lead.shots_per_match <= 32.0);
    REQUIRE(lead.sot_pct >= 35.0);
    REQUIRE(lead.sot_pct <= 48.0);
    REQUIRE(lead.pass_pct >= 82.0);
    REQUIRE(lead.pass_pct <= 89.0);
    REQUIRE(lead.home_poss_pct >= 58.0);
    REQUIRE(lead.home_poss_pct <= 66.0);
}
#else
TEST_CASE("Stage 4 Debug Smoke - Multi-Threaded Calibration Sanity Check", "[calibration]") {
    const auto eq_matches = run_parallel_matches(16, 8, 12, 12, 0xDEADBEEFULL);
    const auto eq = summarize_batch(eq_matches, 0.0);
    REQUIRE(eq.goals_per_match >= 1.5);
    REQUIRE(eq.goals_per_match <= 4.2);
    REQUIRE(eq.shots_per_match >= 15.0);
    REQUIRE(eq.shots_per_match <= 32.0);
    REQUIRE(eq.pass_pct >= 75.0);
    REQUIRE(eq.pass_pct <= 89.0);

    const auto lead_matches = run_parallel_matches(16, 8, 16, 12, 0xDEADBEEFULL);
    const auto lead = summarize_batch(lead_matches, 0.0);
    REQUIRE(lead.home_win_pct >= 62.5);
    REQUIRE(lead.home_poss_pct >= 55.0);
}
#endif

