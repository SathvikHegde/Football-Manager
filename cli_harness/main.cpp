#include <iostream>
#include "fm/engine/version.hpp"
#include "fm/model/match_state.hpp"
#include "fm/engine/match_simulator.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "========================================" << std::endl;
    std::cout << "Football Manager Engine Simulation CLI" << std::endl;
    std::cout << "Status:  " << fm::engine_status() << std::endl;
    std::cout << "Version: " << fm::engine_version() << std::endl;
    std::cout << "========================================" << std::endl;

    fm::MatchSimulator sim;
    fm::MatchState state{};
    sim.reset(12345);

    std::cout << "CLI harness successfully linked against fm_core static library." << std::endl;
    return 0;
}
