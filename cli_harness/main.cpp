#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include "fm/engine/version.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/model/match_event.hpp"
#include "fm/model/match_state.hpp"
#include "fm/recording/match_recording.hpp"

int main(int argc, char* argv[]) {
    uint64_t seed = 12345;
    if (argc > 1) {
        seed = std::strtoull(argv[1], nullptr, 10);
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Football Manager Engine Simulation CLI" << std::endl;
    std::cout << "Status:  " << fm::engine_status() << std::endl;
    std::cout << "Version: " << fm::engine_version() << std::endl;
    std::cout << "Seed:    " << seed << std::endl;
    std::cout << "========================================" << std::endl;

    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;

    const auto start = std::chrono::steady_clock::now();
    sim.initialize_match(state, seed, fm::FormationId::F_442, fm::FormationId::F_433);
    sim.simulate_match(state, recording, fm::Pitch::TICKS_FULL_TIME);
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    int passes = 0;
    int pass_complete = 0;
    int interceptions = 0;
    int tackles = 0;
    int dribbles = 0;
    int shots = 0;
    int shots_on_target = 0;
    int saves = 0;
    int goals = 0;
    int throw_ins = 0;
    int corners = 0;
    int goal_kicks = 0;

    for (const auto& evt : recording.events()) {
        switch (evt.type) {
            case fm::EventType::Pass:            passes++; break;
            case fm::EventType::PassComplete:    pass_complete++; break;
            case fm::EventType::PassIntercepted: interceptions++; break;
            case fm::EventType::TackleWon:       tackles++; break;
            case fm::EventType::Dribble:         dribbles++; break;
            case fm::EventType::Shot:            shots++; break;
            case fm::EventType::ShotOnTarget:    shots_on_target++; break;
            case fm::EventType::Save:            saves++; break;
            case fm::EventType::Goal:            goals++; break;
            case fm::EventType::ThrowIn:         throw_ins++; break;
            case fm::EventType::Corner:          corners++; break;
            case fm::EventType::GoalKick:        goal_kicks++; break;
            default: break;
        }
    }

    const double pass_pct = (passes > 0) ? (100.0 * static_cast<double>(pass_complete) / static_cast<double>(passes)) : 0.0;

    std::cout << "Simulated Frames: " << recording.frame_count() << " (" << std::fixed << std::setprecision(2) << elapsed_ms << " ms)" << std::endl;
    std::cout << "Final Score:      Home " << static_cast<int>(state.score_home)
              << " - " << static_cast<int>(state.score_away) << " Away" << std::endl;
    std::cout << "Shots (On Target):" << shots << " (" << shots_on_target << ")" << std::endl;
    std::cout << "Saves:            " << saves << std::endl;
    std::cout << "Goals Logged:     " << goals << std::endl;
    std::cout << "Passes Completed: " << pass_complete << " / " << passes
              << " (" << std::setprecision(1) << pass_pct << "%)" << std::endl;
    std::cout << "Interceptions:    " << interceptions << " | Tackles Won: " << tackles << std::endl;
    std::cout << "Set Pieces:       Throw-Ins=" << throw_ins << ", Corners=" << corners << ", GoalKicks=" << goal_kicks << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
